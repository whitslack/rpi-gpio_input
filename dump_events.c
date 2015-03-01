#include <stdio.h>

#include <linux/input.h>

int main() {
	struct input_event event;
	unsigned int timestamp = 0, level = 0;
	for (;;) {
		if (fread(&event, sizeof event, 1, stdin) != 1) {
			return 1;
		}
		switch (event.type) {
			case EV_SYN:
				switch (event.code) {
					case SYN_REPORT:
						printf("%u\t%u\n", timestamp, !!level);
						break;
					case SYN_DROPPED:
						fprintf(stderr, "dropped %u events!\n", event.value);
						break;
				}
				level = timestamp = 0;
				break;
			case EV_MSC:
				switch (event.code) {
					case MSC_TIMESTAMP:
						timestamp = event.value;
						break;
				}
				break;
			case EV_SW:
				switch (event.code) {
					case SW_MAX:
						level = event.value;
						break;
				}
				break;
		}
	}
}
