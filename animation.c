#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#include <time.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define PI 3.14159265354

static char prompt[] =
"====================================================\n"
"             L-system Animation                     \n"
"                                                    \n"
"                                                    \n"
"      PRESS   1: Koch snowflake animation           \n"
"      PRESS   2: Fractl plant animation             \n"
"      PRESS   3: Probabilistic animation            \n"
"      PRESS   4: Sierpinski triangle animation      \n"
"      PRESS   5: Dragon curve animation             \n"
"      PRESS   b: Starfield animation                \n"
"      PRESS   f: Toggle Fullscreen                  \n"
"      PRESS   c: Clean screen                       \n"
"      PRESS ESC: Quit window                        \n"
"                                                    \n"
"====================================================\n";

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Surface *surface;
static SDL_Texture *texture;

#define TICK_AVERAGE 120

static int tick_idx, items;
static Uint64 last_tick;
static float elapsed;
static Uint64 during_ticks[TICK_AVERAGE];

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

static int window_width = WINDOW_WIDTH;
static int window_height = WINDOW_HEIGHT;
static bool window_fullscreen = false;

#define LINE_LEN 10

struct node {
	char c;
	struct node *prev;
	struct node *next;
};

static struct node *first;
static float x, y;
static int angle;
static int r, g, b;
static float sum;

static int iterate_times;
static int run_class;

static int times;
static void command(const char *str)
{
	printf("==> %d. %s\n", ++times, str);
	fflush(stdout);
}

static void callbackinfo(const char *str)
{
	int x = 0;
	int t = times;
	while (t) {
		++x;
		t /= 10;
	}

	for (int i = 0; i < x + 6; ++i)
		printf(" ");
	puts(str);
	fflush(stdout);
}

typedef struct TinyAlloc {
	uint8_t *p;
	uint8_t *bufend;
	struct TinyAlloc *next;
	unsigned nb_allocs;
	unsigned size;
	union {
		uint8_t buffer[1];
		size_t _aligner_;
	};
} TinyAlloc;

static TinyAlloc *mempool;

typedef struct tal_header_t {
	size_t size;
} tal_header_t;

#define TAL_ALIGN(size) \
	(((size) + (sizeof (size_t) - 1)) & ~(sizeof (size_t) - 1))

static TinyAlloc *tal_new(TinyAlloc **pal, unsigned size)
{
	TinyAlloc *al = malloc(sizeof(TinyAlloc) - sizeof (size_t) + size);
	al->p = al->buffer;
	al->bufend = al->buffer + size;
	al->nb_allocs = 0;
	al->next = *pal, *pal = al;
	al->size = al->next ? al->next->size : size;
	return al;
}

static void tal_delete(TinyAlloc **pal)
{
	TinyAlloc *al = *pal, *next;

tail_call:
	next = al->next;
	free(al);
	al = next;
	if (al)
		goto tail_call;
	*pal = al;
}

static void tal_free_impl(TinyAlloc **pal, void *p)
{
	TinyAlloc *al, **top = pal;
	tal_header_t *header;

	if (!p)
		return;
	header = (tal_header_t *)p - 1;
	al = *pal;
	while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
		al = *(pal = &al->next);
	if (0 == --al->nb_allocs) {
		*pal = al->next;
		if ((al->bufend - al->buffer) > al->size) {
			free(al);
		} else {
			al->p = al->buffer;
			al->next = *top, *top = al;
		}
	} else if ((uint8_t*)p + header->size == al->p) {
		al->p = (uint8_t*)header;
	}
}

static void *tal_realloc_impl(TinyAlloc **pal, void *p, unsigned size)
{
	tal_header_t *header;
	void *ret;
	unsigned adj_size = TAL_ALIGN(size) + sizeof(tal_header_t);
	TinyAlloc *al = *pal;

	if (p) {
		while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
			al = al->next;
		header = (tal_header_t *)p - 1;
		if ((uint8_t*)p + header->size == al->p)
			al->p = (uint8_t*)header;
		if (al->p + adj_size > al->bufend) {
			ret = tal_realloc_impl(pal, 0, size);
			memcpy(ret, p, header->size);
			tal_free_impl(pal, p);
			return ret;
		} else if (al->p != (uint8_t*)header) {
			memcpy((tal_header_t*)al->p + 1, p, header->size);
		}
	} else {
		while (al->p + adj_size > al->bufend) {
			al = al->next;
			if (!al) {
				unsigned new_size = (*pal)->size;
				if (adj_size > new_size) {
					new_size = adj_size;
				}
				al = tal_new(pal, new_size);
				break;
			}
		}
		al->nb_allocs++;
	}
	header = (tal_header_t *)al->p;
	header->size = adj_size - sizeof(tal_header_t);
	al->p += adj_size;
	ret = header + 1;
	return ret;
}

#define malloc(size) tal_realloc_impl(&mempool, NULL, (size))
#define realloc(ptr, size) tal_realloc_impl(&mempool, (ptr), (size))
#define free(ptr) tal_free_impl(&mempool, (ptr))

struct state {
	float x, y;
	int angle, r, g, b;
};
struct {
	int pointer;
	int depth;
	struct state data[];
} *stack;

static void stack_push(void)
{
	if (stack == NULL) {
		stack = malloc(sizeof *stack + 8 * sizeof(struct state));
		stack->pointer = 0;
		stack->depth   = 8;
	} else if (stack->pointer == stack->depth) {
		stack = realloc(stack, (stack->depth << 1) * sizeof(struct state));
		stack->depth <<= 1;
	}
	stack->data[stack->pointer].x = x;
	stack->data[stack->pointer].y = y;
	stack->data[stack->pointer].angle = angle;
	stack->data[stack->pointer].r = r;
	stack->data[stack->pointer].g = g;
	stack->data[stack->pointer].b = b;
	stack->pointer ++;
}

static void stack_pop(void)
{
	stack->pointer --;
	x     = stack->data[stack->pointer].x;
	y     = stack->data[stack->pointer].y;
	angle = stack->data[stack->pointer].angle;
	r = stack->data[stack->pointer].r;
	g = stack->data[stack->pointer].g;
	b = stack->data[stack->pointer].b;
}

static bool line_complete;
static bool is_completed;

#define FLOAT_EQUAL(f1, f2) (fabs((f1)-(f2)) < 0.0001f)

/* static void print(void) */
/* { */
/* 	for (struct node *n = first; n != NULL; n = n->next) */
/* 		putchar(n->c); */
/* 	puts(""); */
/* } */

static bool show_bg;

#define NUM_POINTS 200
#define CENTERX (window_width  / 2)
#define CENTERY (window_height / 2)
#define RADIUS 20

#define random_float(min, max) (((float)rand()/(float)RAND_MAX) * ((max)-(min)) + (min))

static SDL_FPoint points[NUM_POINTS];
static float point_directionx[NUM_POINTS];
static float point_directiony[NUM_POINTS];

static bool sdf_circle(float x, float y)
{
	float diffx = x - (float)CENTERX;
	float diffy = y - (float)CENTERY;

	return diffx*diffx + diffy*diffy < RADIUS*RADIUS;
}

static void set_point_out_of_screen(int i)
{
	/* points[i].x = -1.f; */
	/* points[i].y = -1.f; */
	points[i].x = window_width << 1;
	points[i].y = window_height << 1;
}

static void set_point_in_circle(int i)
{
	float x;
	float y;

	if (rand() % 800 != 0) {
		set_point_out_of_screen(i);
		return;
	}

retry:
	x = random_float(CENTERX - RADIUS, CENTERX + RADIUS);
	y = random_float(CENTERY - RADIUS, CENTERY + RADIUS);
	if (!sdf_circle(x, y))
		goto retry;

	points[i].x = x;
	points[i].y = y;

	point_directionx[i] = (x - CENTERX) > 0.f ? random_float(0.f, 1.f) : -random_float(0.f, 1.f);
	point_directiony[i] = (y - CENTERY) > 0.f ? random_float(0.f, 1.f) : -random_float(0.f, 1.f);
}

static float point_distance(int i)
{
	float a = points[i].x-(float)CENTERX;
	float b = points[i].y-(float)CENTERY;
	return sqrt(a*a+b*b);
}

static void update_point_position(int i)
{
	const float distance = elapsed * point_distance(i) / 10.f;
	points[i].x += distance * (distance > 1.f ? distance : 1.f) * point_directionx[i];
	points[i].y += distance * (distance > 1.f ? distance : 1.f) * point_directiony[i];
}

static void starfield_construct(void)
{
	int i;

	show_bg = true;
	srand(time(NULL));

	for (i = 0; i < SDL_arraysize(points); ++i)
		set_point_in_circle(i);
}

static void starfield_show(void)
{
	int i;

	SDL_SetRenderDrawColor(renderer, 0, 0, 30, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);

	for (i = 0; i < SDL_arraysize(points); ++i) {
		update_point_position(i);
		if (points[i].x >= window_width || points[i].x < 0.f ||
		    points[i].y >= window_height || points[i].y <= 0.f)
			set_point_in_circle(i);
	}

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	/* SDL_SetRenderDrawColor(renderer, 200, 200, 255, 180); */
	SDL_RenderPoints(renderer, points, SDL_arraysize(points));
}

static void clean_screen(void)
{
	show_bg = false;
	run_class = 0;
	texture = NULL;
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
}

static void draw_line(void)
{
	if (sum >= 1.f) {
		line_complete = true;
		sum = 0.f;
		return;
	}
	float rad = angle / 180.f * PI;
	float nx =  x + LINE_LEN * elapsed * cosf(rad);
	float ny =  y - LINE_LEN * elapsed * sinf(rad);

	SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);

	SDL_RenderLine(renderer, x, y, nx, ny);

	x = nx;
	y = ny;

	sum += elapsed;
}

static int koch_snowflake_draw(void)
{
	struct node *prev;

	if (!line_complete) {
		draw_line();
		return 0;
	} else {
		if (!first)
			return 1;
		switch (first->c) {
		case 'F':
			line_complete = false; draw_line(); break;
		case '+':
			angle += 60; break;
		case '-':
			angle -= 60; break;
		default:
			return 1;
		}
		prev = first;
		first = first->next;
		free(prev);
		return 0;
	}
}

static void koch_snowflake_iterate(struct node *n)
{
	struct node *a, *b, *c, *d, *e, *f, *g, *h;
	if (n->c == 'F') {
		a = malloc(sizeof(struct node));
		b = malloc(sizeof(struct node));
		c = malloc(sizeof(struct node));
		d = malloc(sizeof(struct node));
		e = malloc(sizeof(struct node));
		f = malloc(sizeof(struct node));
		g = malloc(sizeof(struct node));
		h = malloc(sizeof(struct node));

		a->c = 'F';
		b->c = '+';
		c->c = 'F';
		d->c = '-';
		e->c = '-';
		f->c = 'F';
		g->c = '+';
		h->c = 'F';

		a->next = b;
		b->next = c;
		c->next = d;
		d->next = e;
		e->next = f;
		f->next = g;
		g->next = h;
		h->next = n->next;

		a->prev = n->prev;
		b->prev = a;
		c->prev = b;
		d->prev = c;
		e->prev = d;
		f->prev = e;
		g->prev = f;
		h->prev = g;

		if (n->next)
			n->next->prev = h;
		if (n->prev)
			n->prev->next = a;

		free(n);

		if (!a->prev)
			first = a;
	}
}

static void koch_snowflake_construct(void)
{
	int i;
	struct node *n, *next;

	clean_screen();

	show_bg = false;
	iterate_times = 4;
	run_class = 1;
	x = 0, y = window_height * 8 / 9;
	angle = 0;
	r = 255, g = 255, b = 255;
	line_complete = true,  is_completed = false;

	if (stack)
		stack->pointer = 0;
	while (first) {
		next = first->next;
		free(first);
		first = next;
	}

	first = malloc(sizeof *first);
	first->c = 'F';
	first->next = NULL;
	first->prev = NULL;

	for (i = 0; i < iterate_times; ++i) {
		n = first, next = n->next;
		while (n) {
			koch_snowflake_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

static int fractal_plant_draw(void)
{
	struct node *prev;

	if (!line_complete) {
		draw_line();
		return 0;
	} else {
		if (!first)
			return 1;
		switch (first->c) {
		case 'F':
			line_complete = false; draw_line(); break;
		case '+':
			angle += 25; break;
		case '-':
			angle -= 25; break;
		case '[':
			stack_push(); break;
		case ']':
			stack_pop(); break;
		case 'X': break;
		default:
			return 1;
		}
		prev = first;
		first = first->next;
		free(prev);
		return 0;
	}
}

static void fractal_plant_iterate(struct node *nn)
{
	struct node *a, *b, *c, *d, *e, *f, *g, *h,
		    *i, *j, *k, *l, *m, *n, *o, *p,
		    *q, *r;
	if (nn->c == 'X') {
		a = malloc(sizeof(struct node));
		b = malloc(sizeof(struct node));
		c = malloc(sizeof(struct node));
		d = malloc(sizeof(struct node));
		e = malloc(sizeof(struct node));
		f = malloc(sizeof(struct node));
		g = malloc(sizeof(struct node));
		h = malloc(sizeof(struct node));
		i = malloc(sizeof(struct node));
		j = malloc(sizeof(struct node));
		k = malloc(sizeof(struct node));
		l = malloc(sizeof(struct node));
		m = malloc(sizeof(struct node));
		n = malloc(sizeof(struct node));
		o = malloc(sizeof(struct node));
		p = malloc(sizeof(struct node));
		q = malloc(sizeof(struct node));
		r = malloc(sizeof(struct node));

		a->c = 'F';
		b->c = '+';
		c->c = '[';
		d->c = '[';
		e->c = 'X';
		f->c = ']';
		g->c = '-';
		h->c = 'X';
		i->c = ']';
		j->c = '-';
		k->c = 'F';
		l->c = '[';
		m->c = '-';
		n->c = 'F';
		o->c = 'X';
		p->c = ']';
		q->c = '+';
		r->c = 'X';

		a->next = b;
		b->next = c;
		c->next = d;
		d->next = e;
		e->next = f;
		f->next = g;
		g->next = h;
		h->next = i;
		i->next = j;
		j->next = k;
		k->next = l;
		l->next = m;
		m->next = n;
		n->next = o;
		o->next = p;
		p->next = q;
		q->next = r;
		r->next = nn->next;

		a->prev = nn->prev;
		b->prev = a;
		c->prev = b;
		d->prev = c;
		e->prev = d;
		f->prev = e;
		g->prev = f;
		h->prev = g;
		i->prev = h;
		j->prev = i;
		k->prev = j;
		l->prev = k;
		m->prev = l;
		n->prev = m;
		o->prev = n;
		p->prev = o;
		q->prev = p;
		r->prev = q;

		if (nn->next)
			nn->next->prev = r;
		if (nn->prev)
			nn->prev->next = a;

		free(nn);

		if (!a->prev)
			first = a;
	} else if (nn->c == 'F') {
		a = malloc(sizeof(struct node));

		a->c = 'F';
		a->next = nn->next;
		a->prev = nn;

		if (nn->next)
			nn->next->prev = a;
		nn->next = a;
	}
}

static void fractal_plant_construct(void)
{
	int i;
	struct node *n, *next;

	clean_screen();

	show_bg = false;
	iterate_times = 5;
	run_class = 2;
	x = window_width / 9, y = window_height;
	angle = 60;
	r = 0, g = 200, b = 0;
	line_complete = true,  is_completed = false;

	if (stack)
		stack->pointer = 0;
	while (first) {
		next = first->next;
		free(first);
		first = next;
	}

	first = malloc(sizeof *first);
	first->c = 'X';
	first->next = NULL;
	first->prev = NULL;

	for (i = 0; i < iterate_times; ++i) {
		n = first, next = n->next;
		while (n) {
			fractal_plant_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

static void update_color(void)
{
	static float sum = 0;
	sum += elapsed;

	if (sum > 0.7f) {
		r -= 2;
		g -= 1;
		b += 2;

		if (r < 0)
			r = 0;
		if (g < 0)
			g = 0;
		if (b > 255)
			b = 255;

		sum = 0.f;
	}
}

static int probabilistic_draw(void)
{
	struct node *prev;

	if (!line_complete) {
		update_color();
		draw_line();
		return 0;
	} else {
		if (!first)
			return 1;
		switch (first->c) {
		case 'F':
			update_color(); line_complete = false; draw_line(); break;
		case '+':
			angle += 25; break;
		case '-':
			angle -= 25; break;
		case '[':
			stack_push(); break;
		case ']':
			stack_pop(); break;
		default:
			return 1;
		}
		prev = first;
		first = first->next;
		free(prev);
		return 0;
	}
}

static void probabilistic_iterate(struct node *nn)
{
	struct node *a, *b, *c, *d, *e, *f, *g, *h,
		    *i, *j, *k;

	if (nn->c != 'F')
		return;
	a = malloc(sizeof(struct node));
	b = malloc(sizeof(struct node));
	c = malloc(sizeof(struct node));
	d = malloc(sizeof(struct node));
	e = malloc(sizeof(struct node));
	f = malloc(sizeof(struct node));

	a->c = 'F';
	b->c = '[';
	d->c = 'F';
	e->c = ']';
	f->c = 'F';

	a->next = b;
	b->next = c;
	c->next = d;
	d->next = e;
	e->next = f;

	a->prev = nn->prev;
	b->prev = a;
	c->prev = b;
	d->prev = c;
	e->prev = d;
	f->prev = e;

	switch (rand() % 3) {
	case 0:
		g = malloc(sizeof(struct node));
		h = malloc(sizeof(struct node));
		i = malloc(sizeof(struct node));
		j = malloc(sizeof(struct node));
		k = malloc(sizeof(struct node));

		c->c = '+';
		g->c = '[';
		h->c = '-';
		i->c = 'F';
		j->c = ']';
		k->c = 'F';

		f->next = g;
		g->next = h;
		h->next = i;
		i->next = j;
		j->next = k;
		k->next = nn->next;

		g->prev = f;
		h->prev = g;
		i->prev = h;
		j->prev = i;
		k->prev = j;

		if (nn->next)
			nn->next->prev = k;
		break;
	case 1:
		c->c = '+';
		f->next = nn->next;
		if (nn->next)
			nn->next->prev = f;
		break;
	case 2:
		c->c = '-';
		f->next = nn->next;
		if (nn->next)
			nn->next->prev = f;
		break;
	}
	if (nn->prev)
		nn->prev->next = a;
	free(nn);
	if (!a->prev)
		first = a;
}

static void probabilistic_construct(void)
{
	int i;
	struct node *n, *next;

	clean_screen();

	show_bg = false;
	iterate_times = 5;
	run_class = 3;
	x = window_width / 2, y = window_height;
	angle = 90;
	r = 255, g = 165, b = 0;
	line_complete = true,  is_completed = false;

	srand(time(NULL));

	if (stack)
		stack->pointer = 0;
	while (first) {
		next = first->next;
		free(first);
		first = next;
	}

	first = malloc(sizeof *first);
	first->c = 'F';
	first->next = NULL;
	first->prev = NULL;

	for (i = 0; i < iterate_times; ++i) {
		n = first, next = n->next;
		while (n) {
			probabilistic_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

static int sierpinski_triangle_draw(void)
{
	struct node *prev;

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	if (!line_complete) {
		draw_line();
		return 0;
	} else {
		if (!first)
			return 1;
		switch (first->c) {
		case 'F':
		case 'G':
			line_complete = false; draw_line(); break;
		case '+':
			angle += 120; break;
		case '-':
			angle -= 120; break;
		default:
			return 1;
		}
		prev = first;
		first = first->next;
		free(prev);
		return 0;
	}
}

static void sierpinski_triangle_iterate(struct node *n)
{
	struct node *a, *b, *c, *d, *e, *f, *g, *h, *i;
	if (n->c == 'F') {
		a = malloc(sizeof(struct node));
		b = malloc(sizeof(struct node));
		c = malloc(sizeof(struct node));
		d = malloc(sizeof(struct node));
		e = malloc(sizeof(struct node));
		f = malloc(sizeof(struct node));
		g = malloc(sizeof(struct node));
		h = malloc(sizeof(struct node));
		i = malloc(sizeof(struct node));

		a->c = 'F';
		b->c = '-';
		c->c = 'G';
		d->c = '+';
		e->c = 'F';
		f->c = '+';
		g->c = 'G';
		h->c = '-';
		i->c = 'F';

		a->next = b;
		b->next = c;
		c->next = d;
		d->next = e;
		e->next = f;
		f->next = g;
		g->next = h;
		h->next = i;
		i->next = n->next;

		a->prev = n->prev;
		b->prev = a;
		c->prev = b;
		d->prev = c;
		e->prev = d;
		f->prev = e;
		g->prev = f;
		h->prev = g;
		i->prev = h;

		if (n->next)
			n->next->prev = i;
		if (n->prev)
			n->prev->next = a;

		free(n);

		if (!a->prev)
			first = a;
	} else if (n->c == 'G') {
		a = malloc(sizeof(struct node));
		b = malloc(sizeof(struct node));

		a->c = 'G';
		b->c = 'G';

		a->next = b;
		b->next = n->next;

		a->prev = n->prev;
		b->prev = a;

		if (n->next)
			n->next->prev = b;
		if (n->prev)
			n->prev->next = a;

		free(n);

		if (!a->prev)
			first = a;
	}
}

static void sierpinski_triangle_construct(void)
{
	int i;
	struct node *n, *next;
	struct node *a, *e, *c, *d;

	clean_screen();

	show_bg = false;
	iterate_times = 6;
	run_class = 4;
	x = window_width / 2, y = window_height;
	angle = 120;
	r = 0, g = 165, b = 165;
	line_complete = true,  is_completed = false;

	if (stack)
		stack->pointer = 0;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}

	first = malloc(sizeof *first);
	first->c = 'F';

	a = malloc(sizeof *a);
	a->c = '-';
	e = malloc(sizeof *e);
	e->c = 'G';
	c = malloc(sizeof *c);
	c->c = '-';
	d = malloc(sizeof *d);
	d->c = 'G';

	first->next = a;
	a->next = e;
	e->next = c;
	c->next = d;
	d->next = NULL;

	first->prev = NULL;
	a->prev = first;
	e->prev = a;
	c->prev = e;
	d->prev = c;

	for (i = 0; i < iterate_times; ++i) {
		n = first, next = n->next;
		while (n) {
			sierpinski_triangle_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

static int dragon_curve_draw(void)
{
	struct node *prev;

	if (!line_complete) {
		draw_line();
		return 0;
	} else {
		if (!first)
			return 1;
		switch (first->c) {
		case 'F':
		case 'G':
			line_complete = false; draw_line(); break;
		case '+':
			angle += 90; break;
		case '-':
			angle -= 90; break;
		default:
			return 1;
		}
		prev = first;
		first = first->next;
		free(prev);
		return 0;
	}
}


static void dragon_curve_iterate(struct node *n)
{
	struct node *a, *b, *c;

	if (n->c == '+' || n->c == '-')
		return;

	a = malloc(sizeof(struct node));
	b = malloc(sizeof(struct node));
	c = malloc(sizeof(struct node));

	a->c = 'F';
	b->c = n->c == 'F' ? '+' : '-';
	c->c = 'G';
	a->next = b;
	b->next = c;
	c->next = n->next;

	a->prev = n->prev;
	b->prev = a;
	c->prev = b;

	if (n->next)
		n->next->prev = c;
	if (n->prev)
		n->prev->next = a;

	free(n);

	if (!a->prev)
		first = a;
}

static void dragon_curve_construct(void)
{
	int i;
	struct node *n, *next;

	clean_screen();

	show_bg = false;
	iterate_times = 8;
	run_class = 5;
	x = window_width / 5, y = window_height / 2;
	angle = 0;
	r = 255, g = 0, b = 255;
	line_complete = true,  is_completed = false;

	if (stack)
		stack->pointer = 0;
	while (first) {
		next = first->next;
		free(first);
		first = next;
	}

	first = malloc(sizeof *first);
	first->c = 'F';
	first->next = NULL;
	first->prev = NULL;

	for (i = 0; i < iterate_times; ++i) {
		n = first, next = n->next;
		while (n) {
			dragon_curve_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

static void update_elapsed(void)
{
	Uint64 now, sum;
	float average;
	int i;

	sum = 0;
	now = SDL_GetTicks();

	during_ticks[tick_idx] = now - last_tick;
	last_tick = now;
	if (items < TICK_AVERAGE)
		++items;

	for (i = 0; i < TICK_AVERAGE; ++i)
		sum += during_ticks[i];

	average = sum / (float)items;
	tick_idx = (tick_idx + 1) % TICK_AVERAGE;

	elapsed = average / 30.f;
}

static void sig_process(int sig)
{
	puts("\nPlease use closing button of window to quit instead of <ctrl-c>");
	fflush(stdout);
}
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	SDL_SetAppMetadata("dcr's practice", "1.0", "com.practice.dcr");

	if (!SDL_Init(SDL_INIT_VIDEO))
		goto init_error;

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	if (!SDL_CreateWindowAndRenderer("my starfield animation",
					 WINDOW_WIDTH, WINDOW_HEIGHT,
					 SDL_WINDOW_RESIZABLE,
					 &window, &renderer))
		goto create_error;

	SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	tal_new(&mempool, 1024);

	signal(SIGINT, sig_process);

	clean_screen();

	puts(prompt);
	fflush(stdout);

	last_tick = SDL_GetTicks();
	return SDL_APP_CONTINUE;

init_error:
	SDL_Log("initialize error: %s", SDL_GetError());
	return SDL_APP_FAILURE;

create_error:
	SDL_Log("create window or renderer error: %s", SDL_GetError());
	return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	switch (event->type) {
	case SDL_EVENT_QUIT:
		command("Quit window");
		fflush(stdout);
		return SDL_APP_SUCCESS;
	case SDL_EVENT_KEY_DOWN:
		switch (event->key.key) {
		case SDLK_ESCAPE:
			command("Quit window");
			fflush(stdout);
			return SDL_APP_SUCCESS;
		case SDLK_1:
			command("Koch snowflake animation");
			fflush(stdout);
			koch_snowflake_construct();
			break;
		case SDLK_2:
			command("Fractl plant animation");
			fflush(stdout);
			fractal_plant_construct();
			break;
		case SDLK_3:
			command("Probabilistic animation");
			fflush(stdout);
			probabilistic_construct();
			break;
		case SDLK_4:
			command("Sierpinski triangle");
			fflush(stdout);
			sierpinski_triangle_construct();
			break;
		case SDLK_5:
			command("Dragon curve triangle");
			fflush(stdout);
			dragon_curve_construct();
			break;
		case SDLK_B:
			command("Starfield animation");
			fflush(stdout);
			starfield_construct();
			break;
		case SDLK_C:
			command("Clean screen");
			fflush(stdout);
			clean_screen();
			break;
		case SDLK_F:
			command("Toggle Fullscreen");
			fflush(stdout);
			clean_screen();
			window_fullscreen = !window_fullscreen;
			SDL_SetWindowFullscreen(window, window_fullscreen);
			/* SDL_GetRenderOutputSize(renderer, &window_width, &window_height); */
			break;
		}
		break;
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	update_elapsed();

	if (texture) {
		SDL_RenderTexture(renderer, texture, NULL, NULL);
	} else if (show_bg)
		starfield_show();
	else {
		switch (run_class) {
		case 1:
			is_completed = koch_snowflake_draw(); break;
		case 2:
			is_completed = fractal_plant_draw(); break;
		case 3:
			is_completed = probabilistic_draw(); break;
		case 4:
			is_completed = sierpinski_triangle_draw(); break;
		case 5:
			is_completed = dragon_curve_draw(); break;
		default:
			return SDL_APP_CONTINUE;
		}
	}

	if (is_completed) {
		surface = SDL_RenderReadPixels(renderer, NULL);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		run_class = 0;
		is_completed = false;
		callbackinfo("Animation is completed");
		fflush(stdout);
	}

	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) { tal_delete(&mempool); }
