#include <math.h>
#include <stdlib.h>

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

#define POW2(x) x*x

/*************
 * CONSTANTS *
 *************/

const Uint32 FRAMES_PER_SECOND = 60;

const float EPSILON = 0.001f;

/* Colors */
const Uint32 BACKGROUND_COLOR = 0x0000FF;
const Uint32 GROUND_COLOR     = 0x808080;
const Uint32 BALL_COLOR       = 0xFFFF00FF;
const Uint32 PLAYER1_COLOR    = 0x00FFFFFF;
const Uint32 PLAYER2_COLOR    = 0xFF00FFFF;
const Uint32 NET_COLOR        = 0xFFFFFFFF;

/* Screen */
const int SCREEN_WIDTH  = 750;
const int SCREEN_HEIGHT = 375;
const int SCREEN_BPP    = 32;

/* Height of play area */
const float PLAY_HEIGHT = 300.;

/* Ball */
const float BALL_RADIUS  = 10.;
const float BALL_GRAVITY = 0.4;

/* Slime */
const float PLAYER_RADIUS     = 36.;
const float PLAYER_EYE_X      = 17.;
const float PLAYER_EYE_Y      = 13.;
const float PLAYER_EYE_RADIUS = 7.;
const float PLAYER_PUP_X      = 14.;
const float PLAYER_PUP_Y      = 12.;
const float PLAYER_PUP_RADIUS = 3.;

/* Slime physics */
const float PLAYER_MOVE_ACC   = 2.;
const float PLAYER_MOVE_FRICT = 0.6;
const float PLAYER_JUMP_VEL   = 11.;
const float PLAYER_GRAVITY    = 0.7;

/* Bounce speeds */
const float MIN_BOUNCE_SPEED  = 3.;
const float MAX_BOUNCE_SPEED  = 13.;

/* Points */
const int MAX_SCORE = 6;
const int POINT_PADDING = 35;
const int POINT_RADIUS = 12;
const int POINT_DELAY = 1000;

/* Net */
const float NET_WIDTH   = 8.;
const float NET_HEIGHT  = 50.;
const float NET_PADDING = 8.;
const float NET_EPSILON = 5.;

/**************
 * STRUCTURES *
 **************/

typedef struct vect {
	float x;
	float y;
} vect_t;

typedef struct ent {
	vect_t pos;
	SDL_Rect rect;
	vect_t acc;
	vect_t vel;
} ent_t;

typedef struct player {
	int human;
	ent_t ent;
} player_t;

typedef struct match {
	player_t p1;
	player_t p2;
	ent_t ball;
} match_t;

typedef struct game {
	match_t match;
	int s1;
	int s2;
	int winner;
} game_t;

/*********
 * STATE *
 *********/

game_t game;

SDL_Surface *screen, *slime1, *slime2, *ball;
SDL_Event event;

SDL_Rect ground, net;

ent_t net_full;

Uint32 ticks;
Uint32 tmp;

int keys[SDLK_LAST];

/***********
 * METHODS *
 ***********/

/* Die on fail */
void dof(int code) {
	if(code) {
		fprintf(stderr, "error: %s\r\n", SDL_GetError());
		exit(code);
	}
}

/* Axis-aligned bounding-box collision */
int aabbox_coll(ent_t *e1, ent_t *e2) {
	if(e1->pos.y + e1->rect.h <= e2->pos.y) {
		return 0;
	}

	if(e1->pos.y >= e2->pos.y + e2->rect.h) {
		return 0;
	}

	if(e1->pos.x + e1->rect.w <= e2->pos.x) {
		return 0;
	}

	if(e1->pos.x >= e2->pos.x + e2->rect.w) {
		return 0;
	}

	return 1;
}

/* Player-ball collision handler */
void pb_coll_handler(player_t *p, ent_t *ball) {
	vect_t new_vel;
	float speed, tmp;

	if(aabbox_coll(&p->ent, ball)) {
		/* Magnitude of current ball + slime speed */
		speed = sqrt( POW2(ball->vel.x) + POW2(ball->vel.y) )
				+ sqrt( POW2(p->ent.vel.x)
				+ POW2(p->ent.vel.y) ) + EPSILON;
		if(speed < MIN_BOUNCE_SPEED) {
			speed = MIN_BOUNCE_SPEED;
		} else if(speed > MAX_BOUNCE_SPEED) {
			speed = MAX_BOUNCE_SPEED;
		}

		/* Direction of movement */
		new_vel.y = (ball->pos.y + BALL_RADIUS) -
				(p->ent.pos.y + PLAYER_RADIUS);
		new_vel.x = (ball->pos.x + BALL_RADIUS) -
				(p->ent.pos.x + PLAYER_RADIUS);


		/* Hit the flat side of the slime; adjust the normal vector */
		if(new_vel.y > 0) {
			if(ball->pos.x >= p->ent.pos.x + 2*PLAYER_RADIUS -
					EPSILON) {
				ball->vel.x = speed;
				ball->vel.y = 0;
			} else if(ball->pos.x + 2*BALL_RADIUS <= p->ent.pos.x -
					EPSILON) {
				ball->vel.x = -speed;
				ball->vel.y = 0;
			} else {
				ball->vel.x = 0;
				ball->vel.y = speed;
			}
		} else {
			tmp = POW2(ball->pos.x + BALL_RADIUS
					- (p->ent.pos.x + PLAYER_RADIUS))
					+ POW2(ball->pos.y + BALL_RADIUS
					- (p->ent.pos.y + PLAYER_RADIUS));
			if(tmp < POW2(BALL_RADIUS + PLAYER_RADIUS)) {
				ball->vel.x = new_vel.x;
				ball->vel.y = new_vel.y;

				tmp = sqrt( POW2(ball->vel.x)
						+ POW2(ball->vel.y) )
						+ EPSILON;

				ball->vel.x = speed * ball->vel.x / tmp;
				ball->vel.y = speed * ball->vel.y / tmp;
			}
		}
	}
}

/* Update entity */
void update_ent(ent_t *ent) {
	ent->vel.x += ent->acc.x;
	ent->vel.y += ent->acc.y;

	ent->pos.x += ent->vel.x;
	ent->pos.y += ent->vel.y;
}

/* Update entity rectangle */
void update_rect(ent_t *ent) {
	ent->rect.x = round(ent->pos.x);
	ent->rect.y = round(ent->pos.y);
}

/* Update player position */
void update_player(player_t *player) {
	update_ent(&player->ent);

	player->ent.vel.x *= PLAYER_MOVE_FRICT;

	if(player->ent.pos.y > PLAY_HEIGHT - PLAYER_RADIUS) {
		player->ent.pos.y = PLAY_HEIGHT - PLAYER_RADIUS;
		player->ent.vel.y = 0;
	}
}

/* Update all rectangles in a match */
void update_match(game_t *game) {
	int *score;

	update_ent(&game->match.ball);

	if(game->match.ball.pos.y > PLAY_HEIGHT - BALL_RADIUS*2) {
		game->match.ball.pos.y = PLAY_HEIGHT - BALL_RADIUS*2;

		if(game->match.ball.pos.x < SCREEN_WIDTH/2 - BALL_RADIUS) {
			score = &game->s2;
			game->winner = 2;
		} else {
			score = &game->s1;
			game->winner = 1;
		}

		(*score)++;
	}

	/* Global player updates */
	update_player(&game->match.p1);
	update_player(&game->match.p2);

	/* Player 1 x bounds */
	if(game->match.p1.ent.pos.x < 0.) {
		game->match.p1.ent.pos.x = 0.;
	} else if(game->match.p1.ent.pos.x
			> SCREEN_WIDTH/2 - NET_WIDTH/2 - 2*PLAYER_RADIUS - 2) {
		game->match.p1.ent.pos.x
			= SCREEN_WIDTH/2 - NET_WIDTH/2 - 2*PLAYER_RADIUS - 2;
	}

	/* Player 2 x bounds */
	if(game->match.p2.ent.pos.x > SCREEN_WIDTH - 2*PLAYER_RADIUS - 1) {
		game->match.p2.ent.pos.x = SCREEN_WIDTH - 2*PLAYER_RADIUS - 1;
	} else if(game->match.p2.ent.pos.x <
			SCREEN_WIDTH/2 + NET_WIDTH/2 + 1) {
		game->match.p2.ent.pos.x = SCREEN_WIDTH/2 + NET_WIDTH/2 + 1;
	}

	/* Ball x bounds */
	if(game->match.ball.pos.x <= 0) {
		game->match.ball.pos.x = 0;
		game->match.ball.vel.x *= -1;
	} else if(game->match.ball.pos.x >= SCREEN_WIDTH - 2*BALL_RADIUS) {
		game->match.ball.pos.x = SCREEN_WIDTH - 2*BALL_RADIUS;
		game->match.ball.vel.x *= -1;
	}

	/* Player-ball collision */
	pb_coll_handler(&game->match.p1, &game->match.ball);
	pb_coll_handler(&game->match.p2, &game->match.ball);

	/* Net collision */
	if(aabbox_coll(&net_full, &game->match.ball)) {
		if(game->match.ball.pos.y + 2 * BALL_RADIUS - NET_EPSILON >
			net_full.pos.y) {
			if(game->match.ball.vel.x > 0.) {
			game->match.ball.pos.x = SCREEN_WIDTH/2
				- NET_WIDTH/2 - 2*BALL_RADIUS;
			} else {
				game->match.ball.pos.x = SCREEN_WIDTH/2
					+ NET_WIDTH/2;
			}
			game->match.ball.vel.x *= -1;
		} else {
			game->match.ball.vel.y *= -1;
			game->match.ball.pos.y = PLAY_HEIGHT - NET_HEIGHT
				- 2*BALL_RADIUS;
		}
	}

	update_rect(&game->match.ball);
	update_rect(&game->match.p1.ent);
	update_rect(&game->match.p2.ent);
}

/* New match */
void new_match(game_t *game) {
	game->match.p1.ent.pos.x  = SCREEN_WIDTH/4 - PLAYER_RADIUS;
	game->match.p1.ent.pos.y  = PLAY_HEIGHT - PLAYER_RADIUS;
	game->match.p1.human = 1;
	game->match.p1.ent.acc.x = 0;
	game->match.p1.ent.vel.x = 0;
	game->match.p1.ent.vel.y = 0;

	game->match.p2.ent.pos.x  = 3*SCREEN_WIDTH/4 - PLAYER_RADIUS;
	game->match.p2.ent.pos.y  = PLAY_HEIGHT - PLAYER_RADIUS;
	game->match.p2.human = 1;
	game->match.p2.ent.acc.x = 0;
	game->match.p2.ent.vel.x = 0;
	game->match.p2.ent.vel.y = 0;

	if(game->winner == 1) {
		game->match.ball.pos.x = SCREEN_WIDTH/4 - BALL_RADIUS;
	} else {
		game->match.ball.pos.x = 3*SCREEN_WIDTH/4 - BALL_RADIUS;
	}

	game->match.ball.pos.y = PLAY_HEIGHT/2;
	game->match.ball.vel.x = 0;
	game->match.ball.vel.y = 0;

	game->winner = 0;

	update_match(game);
}

/* New game */
void new_game(game_t *game) {
	game->winner = 1;

	game->s1 = 0;
	game->s2 = 0;

	game->match.ball.rect.w = BALL_RADIUS*2;
	game->match.ball.rect.h = BALL_RADIUS*2;
	game->match.ball.acc.y  = BALL_GRAVITY;
	game->match.ball.acc.x  = 0;

	game->match.p1.ent.rect.w = 2*PLAYER_RADIUS;
	game->match.p1.ent.rect.h = PLAYER_RADIUS;
	game->match.p1.ent.acc.y  = PLAYER_GRAVITY;

	game->match.p2.ent.rect.w = 2*PLAYER_RADIUS;
	game->match.p2.ent.rect.h = PLAYER_RADIUS;
	game->match.p2.ent.acc.y  = PLAYER_GRAVITY;

	new_match(game);
}

/* Prepares slime graphic */
SDL_Surface *create_slime(Uint32 color, int left) {
	SDL_Surface *slime;

	slime = SDL_CreateRGBSurface(SDL_HWSURFACE,
			2*PLAYER_RADIUS+1, PLAYER_RADIUS, SCREEN_BPP,
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	filledCircleColor(slime, PLAYER_RADIUS, PLAYER_RADIUS,
			PLAYER_RADIUS, color);
	filledCircleColor(slime,
			left ? (2*PLAYER_RADIUS - PLAYER_EYE_X) : PLAYER_EYE_X,
			PLAYER_EYE_Y, PLAYER_EYE_RADIUS, 0xFFFFFFFF);
	filledCircleColor(slime,
			left ? (2*PLAYER_RADIUS - PLAYER_PUP_X) : PLAYER_PUP_X,
			PLAYER_PUP_Y, PLAYER_PUP_RADIUS, 0x000000FF);

	return slime;
}

void draw_score(SDL_Surface *screen, game_t *game, int left) {
	int i, x0, x, dx, score;
	Uint32 color;

	if(left) {
		score = game->s1;
		x0 = POINT_PADDING;
		dx = POINT_PADDING;
		color = PLAYER1_COLOR;
	} else {
		score = game->s2;
		x0 = SCREEN_WIDTH - POINT_PADDING;
		dx = -POINT_PADDING;
		color = PLAYER2_COLOR;
	}

	for(i = 0, x = x0; i < score; i++, x += dx) {
		filledCircleColor(screen, x, POINT_PADDING, POINT_RADIUS,
				color);
	}

	for(i = 0, x = x0; i < MAX_SCORE; i++, x += dx) {
		circleColor(screen, x, POINT_PADDING, POINT_RADIUS,
				0xFFFFFFFF);
	}
}

void free_resources() {
	SDL_FreeSurface(slime1);
	SDL_FreeSurface(slime2);
	SDL_FreeSurface(ball);
	SDL_Quit();
}

/********
 * MAIN *
 ********/

int main() {
	dof( SDL_Init(SDL_INIT_EVERYTHING) );

	dof( ( screen=SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT,
		SCREEN_BPP, SDL_HWSURFACE) ) == NULL );

	/* Reset keys held */
	memset(keys, 0, sizeof(int) * SDLK_LAST);

	/* New game */
	new_game(&game);

	/* Prepare slime surface */
	slime1 = create_slime(PLAYER1_COLOR, 1);
	slime2 = create_slime(PLAYER2_COLOR, 0);

	/* Prepare ball surface */
	ball = SDL_CreateRGBSurface(SDL_HWSURFACE,
			2*BALL_RADIUS+1, 2*BALL_RADIUS+1, SCREEN_BPP,
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	filledCircleColor(ball, BALL_RADIUS, BALL_RADIUS,
			BALL_RADIUS, BALL_COLOR);

	/* Ground rectangle */
	ground.x = 0;
	ground.y = PLAY_HEIGHT;
	ground.w = SCREEN_WIDTH;
	ground.h = SCREEN_HEIGHT - PLAY_HEIGHT;

	/* Net rectangle */
	net.x = SCREEN_WIDTH/2 - NET_WIDTH/2;
	net.y = PLAY_HEIGHT - NET_HEIGHT;
	net.w = NET_WIDTH;
	net.h = NET_HEIGHT + NET_PADDING;

	net_full.pos.x   = SCREEN_WIDTH/2 - NET_WIDTH/2;
	net_full.pos.y   = PLAY_HEIGHT - NET_HEIGHT;
	net_full.rect.w  = NET_WIDTH;
	net_full.rect.h  = NET_HEIGHT;

	/* match loop */
	ticks = SDL_GetTicks();
	for(;;) {
		SDL_FillRect(screen, NULL, BACKGROUND_COLOR);
		SDL_FillRect(screen, &ground, GROUND_COLOR);
		SDL_FillRect(screen, &net, NET_COLOR);

		/* Events */
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_KEYDOWN) {
				keys[event.key.keysym.sym] = 1;
			} else if(event.type == SDL_KEYUP) {
				keys[event.key.keysym.sym] = 0;
			} else if(event.type == SDL_QUIT) {
				free_resources();
				return 0;
			}

			if(event.key.keysym.sym == SDLK_f) {
				/* TODO: Fullscreen */
				/* SDL_WM_ToggleFullScreen(screen); */
			}
		}

		/* Player 1 */
		game.match.p1.ent.acc.x = 0;
		if(keys[SDLK_a]) {
			game.match.p1.ent.acc.x = -PLAYER_MOVE_ACC;
		} else if(keys[SDLK_d]) {
			game.match.p1.ent.acc.x = PLAYER_MOVE_ACC;
		}
		if(keys[SDLK_w] && game.match.p1.ent.pos.y >
				PLAY_HEIGHT - PLAYER_RADIUS - EPSILON) {
			game.match.p1.ent.vel.y = -PLAYER_JUMP_VEL;
		}

		/* Player 2 */
		game.match.p2.ent.acc.x = 0;
		if(keys[SDLK_LEFT]) {
			game.match.p2.ent.acc.x = -PLAYER_MOVE_ACC;
		} else if(keys[SDLK_RIGHT]) {
			game.match.p2.ent.acc.x = PLAYER_MOVE_ACC;
		}
		if(keys[SDLK_UP] && game.match.p2.ent.pos.y >
				PLAY_HEIGHT - PLAYER_RADIUS - EPSILON) {
			game.match.p2.ent.vel.y = -PLAYER_JUMP_VEL;
		}

		/* Synchronize to frame rate */
		tmp = SDL_GetTicks() - ticks;
		if( tmp < 1000 / FRAMES_PER_SECOND ) {
			SDL_Delay(1000 / FRAMES_PER_SECOND - tmp);
		}
		ticks = SDL_GetTicks();

		/* Update match */
		update_match(&game);

		/* Draw objects to screen */
		draw_score(screen, &game, 1);
		draw_score(screen, &game, 0);
		SDL_BlitSurface(slime1, NULL, screen, &game.match.p1.ent.rect);
		SDL_BlitSurface(slime2, NULL, screen, &game.match.p2.ent.rect);
		SDL_BlitSurface(ball,   NULL, screen, &game.match.ball.rect);

		/* Double buffer flip */
		SDL_Flip(screen);

		if(game.winner) {
			SDL_Delay(POINT_DELAY);

			if(game.s1 >= MAX_SCORE || game.s2 >= MAX_SCORE) {
				new_game(&game);
			} else {
				new_match(&game);
			}
		}
	}

	/* How did you get here? */
	return 1;
}
