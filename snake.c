#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEBUG true

#define cols 21
#define rows 21

#define BASE_TPS 5
#define FOOD_SPEEDUP 0.333

#define HEAD '@'
#define BODY 's'
#define FOOD '*'
#define WALL '#'
#define NONE ' '

#define ANSI_COLOR_RED   "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

typedef enum { false, true } bool;

const int boardlen = cols * rows;
char board[boardlen];

char* death_text = NULL;
int death_progress = 0;
bool running = true;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int snake[boardlen]; // ring buffer containing snake guts
int snake_head = 0;
int snakeX, snakeY = 0;
int snake_length = 300;
bool dead_snake[boardlen];

char direction = '\0';

#define inputcap 3
char inputs[inputcap]; // ring buffer input queue
int inputs_tail = 0;
int inputs_head = 0;

int ticks = 0;
float tps = BASE_TPS;

bool is_running() {
  pthread_mutex_lock(&mutex);
  bool retval = !death_text && running;
  pthread_mutex_unlock(&mutex);
  return retval;
}

void stop() {
  pthread_mutex_lock(&mutex);
  running = false;
  pthread_mutex_unlock(&mutex);
}

void enq_input(char ch) {
  pthread_mutex_lock(&mutex);
  if (inputs_head - inputs_tail < inputcap) {
    inputs[inputs_head % inputcap] = ch;
    inputs_head++;
  }
  pthread_mutex_unlock(&mutex);
}

bool has_input() {
  return inputs_tail < inputs_head;
}

char deq_input() {
  char val = '\0';
  if (has_input()) {
    val = inputs[inputs_tail % inputcap];
    inputs_tail++;
  }
  return val;
}

void snake_record(int val) {
  snake_head = (snake_head + 1) % boardlen;
  snake[snake_head] = val;
}

int snake_query(int distance) {
  return snake[(snake_head + boardlen - distance) % boardlen];
}

void snake_init(int val) {
  for (int h = 0; h < boardlen; h++) {
    snake[h] = -1;
  }
  snake_head = snake_length;
  snake_record(val);
}


int get_pos(int x, int y) {
  return y * cols + x;
}

bool is_in_bounds(int x, int y) {
  return x >= 1 && x < cols - 1 && y >= 1 && y < rows - 1;
}


void init_board() {
  snakeX = (cols - 1) / 2;
  snakeY = (rows - 1) / 2;

  int x, y;
  for (y = 0; y < rows; y++) {
    for (x = 0; x < cols; x++) {
      int pos = get_pos(x, y);
      char ch = NONE;

      if (x == snakeX && y == snakeY) {
        ch = HEAD;
        snake_init(pos);
      } else if (!is_in_bounds(x, y)) {
        ch = WALL;
      }

      board[pos] = ch;
    }
  }
}

void place_food() {
  int places[boardlen];
  int nplaces = 0;
  for (int pos=0; pos<boardlen; pos++) {
    if (board[pos] == NONE) {
      places[nplaces] = pos;
      nplaces++;
    }
  }
  if (nplaces < 1) {
    if (DEBUG) printf("\r\nno food locations!\r\n");
    stop();
    return;
  }

  int pos = places[rand() % nplaces];
  board[pos] = FOOD;
}

void die(char* text) {
  pthread_mutex_lock(&mutex);
  death_text = text;
  pthread_mutex_unlock(&mutex);
}

bool is_direction_change(char newd) {
  switch(newd) {
    case 'w':
    case 's':
      return direction != 'w' && direction != 's';
    case 'a':
    case 'd':
      return direction != 'a' && direction != 'd';
    default:
      return false;
  }
}

void tick() {
  pthread_mutex_lock(&mutex);
  while (has_input()) {
    char newdir = deq_input();
    if (is_direction_change(newdir)) {
      direction = newdir;
      break;
    }
  }
  pthread_mutex_unlock(&mutex);

  switch (direction) {
    case 'w': snakeY -= 1; break;
    case 'a': snakeX -= 1; break;
    case 's': snakeY += 1; break;
    case 'd': snakeX += 1; break;
    default: return;
  }

  ticks++;

  int head = get_pos(snakeX, snakeY);
  snake_record(head);
  int neck = snake_query(1);
  int tail = snake_query(snake_length);

  if (!is_in_bounds(snakeX, snakeY)) {
    die("That wall came out of nowhere.");
    return;
  }
  if (board[head] == BODY && head != tail) {
    die("Ouroboros.");
    return;
  }

  bool ate = board[head] == FOOD;
  board[head] = HEAD;
  if (tail > -1) board[tail] = NONE;
  if (neck > -1 && board[neck] == HEAD) board[neck] = BODY;
  if (ate) {
    tps += FOOD_SPEEDUP;
    snake_length++;
    place_food();
  }
}

void print_centered(char* text) {
  int space = (cols - strlen(text)) / 2;
  for (int i=0; i<space; i++) putchar(' ');
  printf("%s", text);
}

void clear_screen() {
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}
void print_debug() {
  printf("\r\n snakeX: %d", snakeX);
  printf("\r\n snakeY: %d", snakeY);
  printf("\r\n snake_length: %d", snake_length);
  printf("\r\n ticks: %d", ticks);
  printf("\r\n death_progress: %d", death_progress);
  printf("\r\n snake_head: %d", snake_head);
  printf("\r\n\r\nsnake: [\r\n  ");
  for (int i=0; i<snake_length; i++) {
    printf("%d, ", snake_query(i));
  }
  printf("\r\n]\r\n");
}

void print_board() {
  if (!DEBUG) clear_screen();
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      int pos = get_pos(x, y);
      char ch = board[pos];
      char* color = NULL;
      if ((ch == HEAD || ch == BODY) && (dead_snake[pos])) {
        color = ANSI_COLOR_RED;
      } else if (ch == FOOD) {
        color = ANSI_COLOR_GREEN;
      }
      if (color) {
        printf("%s", color);
        putchar(ch);
        printf(ANSI_COLOR_RESET);
      } else {
        putchar(ch);
      }
    }
    printf("\r\n");
  }
  if (DEBUG) print_debug();
}

void print_game_over() {
  printf("\r\n");
  print_centered("GAME OVER");
  printf("\r\n\r\n");
  print_centered(death_text);
  printf("\r\n\r\n");
}


void* gather_input(void* param) {
  while(is_running()) {
    char c = getchar();
    switch (c) {
      case '\033':
        // map arrow keys to wasd
        getchar(); // skip '['
        char arrow = getchar();
        switch (arrow) { // double switch O.o
          case 'A': c = 'w'; break;
          case 'B': c = 's'; break;
          case 'C': c = 'd'; break;
          case 'D': c = 'a'; break;
        }
        // don't break! rare intentional fallthrough.
      case 'w':
      case 'a':
      case 's':
      case 'd':
        enq_input(c);
        break;
      case 'x':
        running = true;
        stop();
        break;
    }
  }
  return NULL;
}

void wait_until(clock_t until) {
  while(clock() < until);
}

int main(int arg, char **argv) {
  srand(time(NULL));
  system("stty raw");

  pthread_t input_thread;
  pthread_create(&input_thread, NULL, gather_input, NULL);
  
  clock_t start = clock();
  clock_t next_tick = start;

  init_board();
  place_food();
  print_board();
  while(is_running()) {
    wait_until(next_tick);
    next_tick += CLOCKS_PER_SEC / (int)tps;
    tick();
    print_board();
  }

  pthread_cancel(input_thread);

  if (death_text) {
    print_game_over();
    for (; death_progress <= snake_length; death_progress++) {
      wait_until(next_tick);
      next_tick += CLOCKS_PER_SEC / (int)tps / 4;
      dead_snake[snake_query(death_progress)] = true;
      print_board();
      print_game_over();
    }
  }
  
  system("stty sane");
  return 0;
}
