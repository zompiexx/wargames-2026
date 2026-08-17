//Wargames Movie Simulator
//Written by Andy Glenn
//(c) 2023

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define COMMAND_LEN 9
#define EXTENSION_LEN 4
#define ACTION_LEN 100
#define MAX_COMMANDS 1000
#define ENTRIES_PER_LINE 4

typedef struct {
    char command[COMMAND_LEN];
    char extension[EXTENSION_LEN];
    char action[ACTION_LEN];
} Command;

Command commands[MAX_COMMANDS];

void fix_backspace_key() {
    system("stty erase '^H'");
}

int audio_enabled(void) {
    const char *silent = getenv("WARGAMES_SILENT");
    return !(silent && strcmp(silent, "1") == 0);
}

void play_sample(const char *sample_path) {
    if (!audio_enabled()) {
        return;
    }

    char command[512];

#ifdef __APPLE__
    snprintf(command, sizeof(command),
             "/usr/bin/afplay \"%s\" >/dev/null 2>&1 &",
             sample_path);
#else
    snprintf(command, sizeof(command),
             "aplay -q \"%s\" >/dev/null 2>&1 &",
             sample_path);
#endif

    // Audio is decorative. Do not block the simulator waiting for the
    // external player process to finish.
    system(command);
}

void play_dtmf_sample(const char *sample_path) {
    if (!audio_enabled()) {
        return;
    }

    char command[512];

#ifdef __APPLE__
    snprintf(command, sizeof(command),
             "/usr/bin/afplay \"%s\" >/dev/null 2>&1 &",
             sample_path);
#else
    snprintf(command, sizeof(command),
             "aplay -q \"%s\" >/dev/null 2>&1 &",
             sample_path);
#endif

    // Launch the tone asynchronously, then let the simulator itself control
    // the dialling cadence. This avoids blocking on afplay/aplay under a PTY.
    system(command);
    usleep(220000);
}

void play_phone_number(const char *phone_number) {
    for (size_t i = 0; phone_number[i] != '\0'; i++) {
        if (isdigit((unsigned char)phone_number[i])) {
            char sample_path[256];

            snprintf(sample_path, sizeof(sample_path),
                     "./samples/%c.wav",
                     phone_number[i]);

            play_dtmf_sample(sample_path);
        }
    }
}

void toLowerCase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void toUpperCase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void readFile() {
    FILE *file;
    int index = 0;

    file = fopen("cpm_commands.txt", "r");

    if (file == NULL) {
        file = fopen("cpm_commands.txt", "w");
        if (file == NULL) {
            perror("Error creating cpm_commands.txt");
            exit(EXIT_FAILURE);
        }
        printf("File cpm_commands.txt created successfully.\n");
        fclose(file);
        return;
    }

    while (index < MAX_COMMANDS &&
           fscanf(file, "%8s\n%3s\n%[^\n]\n",
                  commands[index].command,
                  commands[index].extension,
                  commands[index].action) == 3) {
        toLowerCase(commands[index].command);
        toLowerCase(commands[index].extension);
        index++;
    }

    fclose(file);
}

void printCommands() {
    char upperCommand[COMMAND_LEN];
    char upperExtension[EXTENSION_LEN];
    int firstCommandInLine = 1;

    for (int i = 0; i < MAX_COMMANDS && commands[i].command[0] != 0; i++) {
        strcpy(upperCommand, commands[i].command);
        strcpy(upperExtension, commands[i].extension);
        toUpperCase(upperCommand);
        toUpperCase(upperExtension);

        if (firstCommandInLine) {
            printf("B: ");
            firstCommandInLine = 0;
        }

        printf("%s", upperCommand);
        for (int j = (int)strlen(upperCommand); j < 8; j++) {
            printf(" ");
        }
        printf(" %.3s", upperExtension);

        if ((i + 1) % ENTRIES_PER_LINE == 0 || commands[i+1].command[0] == 0) {
            printf("\n");
            firstCommandInLine = 1;
        } else {
            printf(" : ");
        }
    }
}

void promptAndExecute(const char *inputCommand) {
    char fullCmd[COMMAND_LEN + EXTENSION_LEN + 1];
    char input[COMMAND_LEN + EXTENSION_LEN + 1];

    strncpy(input, inputCommand, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';

    toLowerCase(input);

    for (int i = 0; i < MAX_COMMANDS && commands[i].command[0] != 0; i++) {
        if (strcmp(input, commands[i].command) == 0) {
            system(commands[i].action);
            return;
        }

        snprintf(fullCmd, sizeof(fullCmd), "%s.%s",
                 commands[i].command, commands[i].extension);

        if (strcmp(input, fullCmd) == 0) {
            system(commands[i].action);
            return;
        }
    }

    toUpperCase(input);
    printf("%s? \n\n", input);
}

int main() {
    fix_backspace_key();
    clear_screen();

    char command[100];
    char prompt[3];

    strcpy(prompt, "A");

    readFile();

    sleep(1);
    printf("64K CP/M VERS. 2.2 MCL030210-D-F8\n\n");

imsai8080:
    printf("%s>", prompt);
    fflush(stdout);

    if (fgets(command, sizeof(command), stdin) == NULL) {
        goto bye;
    }

    command[strcspn(command, "\r\n")] = '\0';

    if (command[0] == '\0') {
        printf("\n");
        goto imsai8080;
    }

    toLowerCase(command);

    if (strcmp(command, "cls") == 0) {
        clear_screen();
        goto imsai8080;

    } else if (strcmp(command, "b:") == 0) {
        strcpy(prompt, "B");
        goto imsai8080;

    } else if (strcmp(command, "a:") == 0) {
        strcpy(prompt, "A");
        goto imsai8080;

    } else if (strcmp(command, "bye") == 0 && strcmp(prompt, "A") == 0) {
        printf("INT disabled and HALT Op-Code reached at 0101\n");
        goto bye;

    } else if (strcmp(command, "dir") == 0 && strcmp(prompt, "A") == 0) {
        printf("A: BYE      COM : CLS      COM : DIALER   COM : DIR      COM\n");
        printf("A: KERMIT   COM\n");
        goto imsai8080;

    } else if (strcmp(command, "dir a:") == 0) {
        printf("A: BYE      COM : CLS      COM : DIALER   COM : DIR      COM\n");
        printf("A: KERMIT   COM\n");
        goto imsai8080;

    } else if (strcmp(command, "dir") == 0 && strcmp(prompt, "B") == 0) {
        printCommands();
        goto imsai8080;

    } else if (strcmp(command, "dir b:") == 0) {
        printCommands();
        goto imsai8080;

    } else if (strcmp(command, "dialer") == 0 && strcmp(prompt, "A") == 0) {
        clear_screen();
        printf("DIALER\n\n");
        fflush(stdout);
        sleep(2);
        system("./dialer");
        goto imsai8080;

    } else if (strcmp(command, "kermit") == 0 && strcmp(prompt, "A") == 0) {
        printf("Kermit-80 v4.11 configured for Generic CP/M-80 with Generic (Dumb) CRT Terminal\n");
        printf("type selected\n\n");
        printf("For help, type ? at any point in a command\n");
        sleep(2);
        printf("Kermit-80   0I:>set port uc1\n");
        sleep(2);

kermit:
        printf("Kermit-80   0I:>");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            goto bye;
        }

        command[strcspn(command, "\r\n")] = '\0';

        if (command[0] == '\0') {
            printf("\n");
            goto kermit;
        }

        toLowerCase(command);

        if (strcmp(command, "?") == 0) {
            printf("CONNECT to host on selected port\n\n");
            sleep(1);
        }

        if (strcmp(command, "quit") == 0) {
            goto imsai8080;
        }

        if (strcmp(command, "connect") == 0) {
            printf("Connected to remote host.  Type Control-C to return\n");
            printf("type Control-? for command list\n");
            sleep(2);

            printf("ATDT3115554855\n");
            fflush(stdout);

            const char my_phone_number[] = "3115554855";
            play_phone_number(my_phone_number);

            usleep(250000);
            play_sample("./samples/1200-modem.wav");

            sleep(5);
            clear_screen();

            printf("CONNECTING\n\n");
            fflush(stdout);
            sleep(5);

            system("./school");
        }

        goto kermit;

    } else if (strcmp(prompt, "B") == 0) {
        promptAndExecute(command);
        goto imsai8080;

    } else {
        toUpperCase(command);
        printf("%s? \n\n", command);
        goto imsai8080;
    }

bye:
    return 0;
}
