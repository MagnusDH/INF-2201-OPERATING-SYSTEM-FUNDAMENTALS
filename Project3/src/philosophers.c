#include "scheduler.h"
#include "screen.h"
#include "thread.h"
#include "util.h"

/* Dining philosphers threads. */

enum
{
	THINK_TIME = 9999,
	EAT_TIME = THINK_TIME,
};

volatile int forks_initialized = 0;
lock_t fork[3];			//Staticly allocate space for 3 forks
condition_t cond_queue;
int num_eating = 0;
int scroll_eating = 0;
int caps_eating = 0;

int num_eating_count = 0;
int caps_eating_count = 0;
int scroll_eating_count = 0;
/* Set to true if status should be printed to screen */
int print_to_screen;

enum
{
	LED_NONE = 0x00,
	LED_SCROLL = 0x01,
	LED_NUM = 0x02,
	LED_CAPS = 0x04,
	LED_ALL = 0x07
};

/* Turns keyboard LEDs on or off according to bitmask.
 *
 * Bitmask is composed of the following three flags:
 * 0x01 -- SCROLL LOCK LED enable flag
 * 0x02 -- NUM LOCK LED enable flag
 * 0x04 -- CAPS LOCK LED enable flag
 *
 * Bitmask = 0x00 thus disables all LEDS, while 0x07
 * enables all LEDS.
 *
 * See http://www.computer-engineering.org/ps2keyboard/
 * and http://forum.osdev.org/viewtopic.php?t=10053
 */
static void update_keyboard_LED(unsigned char bitmask) {
	/* Make sure that bitmask only contains bits for status LEDs  */
	bitmask &= 0x07;

	/* Wait for keyboard buffer to be empty */
	while (inb(0x64) & 0x02)
		;
	/* Tells the keyboard to update LEDs */
	outb(0x60, 0xed);
	/* Wait for the keyboard to acknowledge LED change message */
	while (inb(0x60) != 0xfa)
		;
	/* Write bitmask to keyboard */
	outb(0x60, bitmask);

	ms_delay(100);
}

static void think_for_a_random_time(void) {
	volatile int foo;
	int i, n;

	n = rand() % THINK_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}

static void eat_for_a_random_time(void) {
	volatile int foo;
	int i, n;

	n = rand() % EAT_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}


/* Odd philosopher */
void num(void) {
	print_to_screen = 1;

	//Initialize condition variables
	condition_init(&cond_queue);
	lock_init(&fork[0]);
	lock_init(&fork[1]);
	lock_init(&fork[2]);
	forks_initialized = 1;

	while (1) {
		think_for_a_random_time();

		//If no other philosophers are eating
		if(caps_eating == 0 && scroll_eating == 0){
			//If neighbour philosophers have eaten the same or more times than this philosopher
			if(caps_eating_count >= num_eating_count && scroll_eating_count >= num_eating_count){
				//grab left fork
				lock_acquire(&fork[0]);
				//grab right fork
				lock_acquire(&fork[1]);

				//Philosopher is now eating
				num_eating = 1;

				num_eating_count++;
			}
			//If neighbour philosophers have eaten less times than this philosopher
			else{
				//Wait
				condition_wait(&fork[0], &cond_queue);
			}
		}

		//If one or both neighbouring philosophers are eating
		if(caps_eating == 1 || scroll_eating == 1){
			//wait
			condition_wait(&fork[0], &cond_queue);
		}


		/* Enable NUM-LOCK LED and disable the others */
		// update_keyboard_LED(LED_NUM);

		/*Print philosoper to screen*/
		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Num    ");
		}

		eat_for_a_random_time();

		//Release forks
		lock_release(&fork[0]);
		lock_release(&fork[1]);

		//Done eating
		num_eating = 0;

		//Release all blocked philosophers
		condition_broadcast(&cond_queue);
		print_str(7, 50, "NUM EATING:");
		print_int(7, 65, num_eating_count);
	}
}

void caps(void) {
	/* Wait until num has initialized forks */
	while (forks_initialized == 0)
		yield();

	while (1) {
		think_for_a_random_time();

		if(num_eating == 0 && scroll_eating == 0){
			if(num_eating_count >= caps_eating_count && scroll_eating_count >= caps_eating_count){
				lock_acquire(&fork[1]);
				lock_acquire(&fork[2]);

				caps_eating = 1;

				caps_eating_count++;
			}
		}


		if(num_eating == 1 || scroll_eating == 1){
			//wait
			condition_wait(&fork[1], &cond_queue);
		}


		/* Enable CAPS-LOCK LED and disable the others */
		// update_keyboard_LED(LED_CAPS);

		/*Print philosoper to screen*/
		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Caps   ");
		}

		eat_for_a_random_time();

		//Release forks
		lock_release(&fork[1]);
		lock_release(&fork[2]);

		//Done eating
		caps_eating = 0;

		condition_broadcast(&cond_queue);

		print_str(8, 50, "CAPS EATING:");
		print_int(8, 65, caps_eating_count);

	}
}

void scroll_th(void) {
	/* Wait until num has initialized forks */
	while (forks_initialized == 0)
		yield();

	while (1) {
		think_for_a_random_time();

		//If no other philosophers are eating
		if(num_eating == 0 && caps_eating == 0){
			//If neighbour philosophers have eaten the same or more times than this philosopher
			if(num_eating_count >= scroll_eating_count && caps_eating_count >= scroll_eating_count){
				//grab left fork
				lock_acquire(&fork[2]);
				//grab right fork
				lock_acquire(&fork[0]);

				//Philosopher is now eating
				scroll_eating = 1;

				scroll_eating_count++;
			}
			//If neighbour philosophers have eaten less times than this philosopher
			else{
				//Wait
				condition_wait(&fork[2], &cond_queue);
			}
		}

		//If one or both neighbouring philosophers are eating
		if(num_eating == 1 || caps_eating == 1){
			//wait
			condition_wait(&fork[2], &cond_queue);
		}


		/* Enable SCROLL-LOCK LED and disable the others */
		// update_keyboard_LED(LED_SCROLL);

		/*Print philosoper to screen*/
		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Scroll ");
		}

		eat_for_a_random_time();

		//Release forks
		lock_release(&fork[2]);
		lock_release(&fork[0]);

        //Done eating
        scroll_eating = 0;

		//Release all blocked philosophers
		condition_broadcast(&cond_queue);

		print_str(9, 50, "SCROLL EATING:");
		print_int(9, 65, scroll_eating_count);
	}
}