#include <stdio.h>
#include <string.h>

int main() {
    char input[100];
    FILE *history;

    while (1) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\nExit (Ctrl+D)\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "\\q") == 0) {
            printf("Exit\n");
            break;
        }
       
	if (strncmp(input, "echo ", 5) == 0) {
	    printf("%s\n", input + 5);
	} else if (strlen(input) > 0) {
            printf("%s: command found\n", input);
	}

	if (strlen(input) > 0) {
            history = fopen(".kubsh_history", "a");
            if (history != NULL) {
                fprintf(history, "%s\n", input);
                fclose(history);
            }
        }
    }
    return 0;
}
