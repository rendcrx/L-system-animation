#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#ifdef PROBABILISTIC
#include <time.h>
#endif

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define PI 3.14159265354

static SDL_Window *window;
static SDL_Renderer *renderer;

#define TICK_AVERAGE 120

static int tick_idx, items;
static Uint64 last_tick;
static float elapsed;
static Uint64 during_ticks[TICK_AVERAGE];

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#define LINE_LEN 10

static SDL_Texture *screen;
static SDL_FRect    rect;

struct node {
	char c;
	struct node *prev;
	struct node *next;
};

static struct node *first;
#ifdef KOCHSNOWFLAKE
static float x, y = WINDOW_HEIGHT * 8 / 9;
static int angle;
#elif FRACTALPLANT
static float x = WINDOW_WIDTH / 9, y = WINDOW_HEIGHT;
static int angle = 60;
#elif PROBABILISTIC
static float x = WINDOW_WIDTH / 2, y = WINDOW_HEIGHT;
static int angle = 90;
#endif

#if defined(FRACTALPLANT) || defined(PROBABILISTIC)
struct state {
	float x, y;
	int angle;
};
struct {
	int pointer;
	int depth;
	struct state data[];
} *stack;

static void stack_push()
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
	stack->pointer ++;
}

static void stack_pop()
{
	stack->pointer --;
	x     = stack->data[stack->pointer].x;
	y     = stack->data[stack->pointer].y;
	angle = stack->data[stack->pointer].angle;
}

#endif

static float sum;
static bool line_complete = true;
static bool is_complete = false;

#define FLOAT_EQUAL(f1, f2) (fabs((f1)-(f2)) < 0.0001f)

static void print()
{
	for (struct node *n = first; n != NULL; n = n->next)
		putchar(n->c);
	puts("");
}

static void draw_line()
{
	if (sum >= 1.f) {
		line_complete = true;
		sum = 0.f;
		return;
	}
	float rad = angle / 180.f * PI;
	float nx =  x + LINE_LEN * elapsed * cosf(rad);
	float ny =  y - LINE_LEN * elapsed * sinf(rad);

	SDL_RenderLine(renderer, x, y, nx, ny);

	x = nx;
	y = ny;

	sum += elapsed;
}

#ifdef KOCHSNOWFLAKE

static int koch_snowflake_draw()
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

static void koch_snowflake_construct(int x)
{
	int i;
	struct node *n, *next;

	first = malloc(sizeof *first);
	first->c = 'F';
	first->next = NULL;

	for (i = 0; i < x; ++i) {
		n = first, next = n->next;
		while (n) {
			koch_snowflake_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

#elif FRACTALPLANT

static int fractal_plant_draw()
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

static void fractal_plant_construct(int x)
{
	int i;
	struct node *n, *next;

	first = malloc(sizeof *first);
	first->c = 'X';
	first->next = NULL;

	for (i = 0; i < x; ++i) {
		n = first, next = n->next;
		while (n) {
			fractal_plant_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

#elif PROBABILISTIC

static int probabilistic_draw()
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
			line_complete = false; draw_line(); break;
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

static void probabilistic_construct(int x)
{
	int i;
	struct node *n, *next;

	srand(time(NULL));

	first = malloc(sizeof *first);
	first->c = 'F';
	first->next = NULL;

	for (i = 0; i < x; ++i) {
		n = first, next = n->next;
		while (n) {
			probabilistic_iterate(n);
			n = next;
			if (n)
				next = n->next;
		}
	}
}

#endif

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
	puts("\nplease use closing button of window to quit instead of <ctrl-c>");
}
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (argc != 2) {
		puts("Usage: ./animation <iterate_number>");
		return SDL_APP_FAILURE;
	}

#ifdef KOCHSNOWFLAKE
	koch_snowflake_construct(atoi(argv[1]));
#elif FRACTALPLANT
	fractal_plant_construct(atoi(argv[1]));
#elif PROBABILISTIC
	probabilistic_construct(atoi(argv[1]));
#endif

	/* print(); */
	/* return SDL_APP_SUCCESS; */

	SDL_SetAppMetadata("dcr's practice", "1.0", "com.practice.dcr");

	if (!SDL_Init(SDL_INIT_VIDEO))
		goto init_error;

	if (!SDL_CreateWindowAndRenderer("my starfield animation",
					 WINDOW_WIDTH, WINDOW_HEIGHT,
					 SDL_WINDOW_RESIZABLE,
					 &window, &renderer))
		goto create_error;

	SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	signal(SIGINT, sig_process);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);

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
	if (event->type == SDL_EVENT_QUIT ||
	    (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE))
		return SDL_APP_SUCCESS;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	update_elapsed();
	if (is_complete) {
		SDL_RenderTexture(renderer, screen, NULL, &rect);
	} else {
#ifdef KOCHSNOWFLAKE
		if (koch_snowflake_draw()) {
#elif FRACTALPLANT
		if (fractal_plant_draw()) {
#elif PROBABILISTIC
		if (probabilistic_draw()) {
#endif
			is_complete = true;
			screen = SDL_CreateTextureFromSurface(renderer, SDL_RenderReadPixels(renderer, NULL));
			rect.x = 0.;
			rect.y = 0.;
			rect.w = (float)WINDOW_WIDTH;
			rect.h = (float)WINDOW_HEIGHT;
			puts("animation is completed");
		}
	}

	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}
