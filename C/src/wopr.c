//Wargames Movie Simulator
//Written by Andy Glenn
//(c) 2023

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <ncurses.h>
#include <termios.h>
#include <stdbool.h>

#define CHARACTER_DELAY 7500  // 1000 = 1ms
#define MAX_TARGETS 4
#define MAX_STRING_LENGTH 20
#define INBOX 1
#define SENT_ITEMS 2

// Struct for user data
typedef struct {
    char username[100];
    char password[100];
    char name[100];
    int access_level;
    char last_logon[100];
} User;

// Define the Mail structure
typedef struct {
    char sender[100];
    char recipient[100];
    char subject[100];
    char message[500];
    char date[15];      // Format: DD-MM-YYYY
    char time[10];      // Format: HH:MM:SS
} Mail;

int game_running = 0;
int defcon = 5;
int hints = 0; // 0 = disabled, 1 = enabled
int wopr_chat_enabled = 1;  // 0 = disabled, 1 = enabled (default)

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
             "/usr/bin/afplay \"%s\" >/dev/null 2>&1",
             sample_path);
#else
    snprintf(command, sizeof(command),
             "aplay -q \"%s\" >/dev/null 2>&1",
             sample_path);
#endif

    // Blocking playback: sequences that use play_sample() deliberately wait
    // for the clip to finish before continuing.
    system(command);
}

void play_sample_async(const char *sample_path) {
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

    // Decorative computer beeps may run underneath terminal output. Voice and
    // other important samples continue to use blocking play_sample().
    system(command);
}

void delayed_print(const char* str) {
    //char command[200];
    for (int i = 0; str[i]; i++) {
        putchar(str[i]);
        fflush(stdout);
        //snprintf(command, sizeof(command), "aplay samples/phone-beep.wav -q &");
        //system(command);
        usleep(CHARACTER_DELAY);
    }
}

void not_delayed_print(const char* str) {
    for (int i = 0; str[i]; i++) {
        putchar(str[i]);
        fflush(stdout);
        usleep(500);
    }
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void press_enter_to_continue(void) {
    char buffer[8];

    delayed_print("PRESS 'ENTER' TO CONTINUE");
    fflush(stdout);

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (buffer[0] == '\n' || buffer[0] == '\r') {
            break;
        }
    }
}

void author() {
    int asciiValues[] = {65, 78, 68, 89, 32, 71, 76, 69, 78, 78};
    int i;
    char command[200];

    play_sample_async("./samples/computer-beeps-short.wav");

    printf("\n");
    for(i = 0; i < 10; i++) {
        printf("%c", asciiValues[i]);
    }
    printf("\n\n");
}

void show_date() {
    char command[200];
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    char date_string[100];
    strftime(date_string, sizeof(date_string), "\nDATE: %Y-%m-%d\n\n", time_info);
    play_sample_async("./samples/computer-beeps-short.wav");
    delayed_print(date_string);
}

void show_time() {
    char command[200];
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    char time_string[100];
    strftime(time_string, sizeof(time_string), "\nTIME: %H:%M:%S\n\n", time_info);
    play_sample_async("./samples/computer-beeps-short.wav");
    delayed_print(time_string);
}     

void show_list() {
    char command[200];
    delayed_print("\nUSE SYNTAX: LIST <TYPE>\n\n");
    play_sample_async("./samples/computer-beeps.wav");
    //snprintf(command, sizeof(command), "espeak 'USE SYNTAX: LIST TYPE'");
    //system(command);
}

void connect_internet() {
        char command[200];
        snprintf(command, sizeof(command), "./internet.sh");
        system(command);

}

void connect_arpanet() {
        char command[200];
        snprintf(command, sizeof(command), "./telehack.sh");
        system(command);

}

void start_wopr_chat_service() {
    if (wopr_chat_enabled != 1) {
        return;
    }

    // Start/ensure the local WOPR Chat bridge in the background while Joshua's
    // scripted startup sequence continues. WOPR Chat owns the local inference
    // engine and waits for it to become ready before servicing a query.
    system("python3 ./wopr_chat/wopr_chat.py --ensure >/dev/null 2>&1 &");
}

static char *strcasestr_local(char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL || *needle == '\0') {
        return haystack;
    }

    size_t needle_len = strlen(needle);
    for (char *p = haystack; *p != '\0'; p++) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return p;
        }
    }

    return NULL;
}

int wopr_chat_response(const char *input, char *action, size_t action_size) {
    char input_path[] = "/tmp/wopr_query_input_XXXXXX";
    int fd = mkstemp(input_path);
    if (fd == -1) {
        delayed_print("\nNEURAL RESPONSE MODULE UNAVAILABLE\n\n");
        return 0;
    }

    FILE *input_file = fdopen(fd, "w");
    if (input_file == NULL) {
        close(fd);
        unlink(input_path);
        delayed_print("\nNEURAL RESPONSE MODULE UNAVAILABLE\n\n");
        return 0;
    }
    fprintf(input_file, "%s\n", input);
    fclose(input_file);

    char command[1024];
    snprintf(command, sizeof(command),
             "python3 ./wopr_chat/wopr_query.py < '%s'", input_path);

    FILE *chat = popen(command, "r");
    if (chat == NULL) {
        unlink(input_path);
        delayed_print("\nNEURAL RESPONSE MODULE UNAVAILABLE\n\n");
        return 0;
    }

    char response[8192] = {0};
    size_t used = 0;
    while (used < sizeof(response) - 1 &&
           fgets(response + used, sizeof(response) - used, chat) != NULL) {
        used = strlen(response);
    }

    int query_status = pclose(chat);
    unlink(input_path);
    if (query_status != 0) {
        delayed_print("\nNEURAL RESPONSE MODULE ERROR\n");
        return 0;
    }

    if (action && action_size > 0) {
        snprintf(action, action_size, "NONE");
    }

    const char *open_tag = "<WOPR_CONTROL>";
    const char *close_tag = "</WOPR_CONTROL>";
    char *tag_start = strcasestr_local(response, open_tag);
    if (tag_start != NULL) {
        char *tag_end = strcasestr_local(tag_start, close_tag);
        if (tag_end != NULL) {
            char saved = *tag_end;
            *tag_end = '\0';
            char *action_line = strcasestr_local(tag_start, "ACTION=");
            if (action_line != NULL && action && action_size > 0) {
                action_line += 7;
                while (*action_line && isspace((unsigned char)*action_line)) {
                    action_line++;
                }
                size_t n = strcspn(action_line, "\r\n< ");
                if (n >= action_size) n = action_size - 1;
                memcpy(action, action_line, n);
                action[n] = '\0';

                // Normalise action names to uppercase so C-side comparisons remain stable.
                for (size_t i = 0; action[i] != '\0'; i++) {
                    action[i] = (char)toupper((unsigned char)action[i]);
                }
            }
            *tag_end = saved;
            memmove(tag_start, tag_end + strlen(close_tag),
                    strlen(tag_end + strlen(close_tag)) + 1);
        }
    }

    // Never expose control metadata to the terminal, even if the model varies
    // tag capitalisation (for example </WOPR_Control>) or emits duplicates.
    while ((tag_start = strcasestr_local(response, open_tag)) != NULL) {
        char *tag_end = strcasestr_local(tag_start, close_tag);
        if (tag_end == NULL) {
            *tag_start = '\0';
            break;
        }
        memmove(tag_start, tag_end + strlen(close_tag),
                strlen(tag_end + strlen(close_tag)) + 1);
    }

    // Trim trailing whitespace left behind by the hidden control block.
    size_t len = strlen(response);
    while (len > 0 && isspace((unsigned char)response[len - 1])) {
        response[--len] = '\0';
    }

    if (len > 0) {
        printf("\n%s\n\n", response);
        fflush(stdout);
    }
    return 1;
}

static int is_joshua_chat_command(const char *input) {
    return strcmp(input, "chat") == 0;
}

static int is_joshua_resume_command(const char *input) {
    return strcmp(input, "resume") == 0 ||
           strcmp(input, "start") == 0;
}

static int is_joshua_local_command(const char *input) {
    return strcmp(input, "help") == 0 ||
           strcmp(input, "help games") == 0 ||
           strcmp(input, "") == 0 ||
           strcmp(input, "list") == 0 ||
           strcmp(input, "internet") == 0 ||
           strcmp(input, "arpanet") == 0 ||
           strcmp(input, "list games") == 0 ||
           strcmp(input, "date") == 0 ||
           strcmp(input, "time") == 0 ||
           strcmp(input, "exit") == 0 ||
           strcmp(input, "author") == 0 ||
           strcmp(input, "defcon") == 0 ||
           strcmp(input, "tic-tac-toe") == 0 ||
           strcmp(input, "cls") == 0;
}

const char *check_status_from_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        // If the file doesn't exist, create it with default content "enabled"
        file = fopen(filename, "w");
        if (!file) {
            perror("Error creating file");
            return "error";
        }
        fprintf(file, "enabled");
        fclose(file);
        
        // Return the default status
        return "enabled";
    }

    char status[10];  // enough to hold "enabled" or "disabled" and a null terminator
    if (fscanf(file, "%9s", status) != 1) {
        fclose(file);
        return "error";
    }

    fclose(file);

    if (strcmp(status, "enabled") == 0) {
        return "enabled";
    } else if (strcmp(status, "disabled") == 0) {
        return "disabled";
    } else {
        return "error";
    }
}

int set_status_to_file(const char *filename, int status_input) {
    const char *status;
    if (status_input == 0) {
        status = "disabled";
    } else if (status_input == 1) {
        status = "enabled";
    } else {
        fprintf(stderr, "Invalid input: %d\n", status_input);
        return -1;  // Return an error code
    }

    // Open the file for writing, which will create it if it doesn't exist
    // or overwrite its content if it does.
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Unable to open file for writing");
        return -1;  // Return an error code
    }

    fprintf(file, "%s", status);
    fclose(file);

    return 0;  // Successful write
}

bool userExists(const char* username) {
    FILE *file = fopen("users.txt", "r");
    if (!file) {
        printf("Error opening users database.\n");
        return false;
    }

    char lineBuffer[100];
    User user;
    while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
        // Trim newline and copy username
        lineBuffer[strcspn(lineBuffer, "\n")] = 0;
        strcpy(user.username, lineBuffer);

        // Compare usernames
        if (strcmp(user.username, username) == 0) {
            fclose(file);
            return true;
        }

        // Skip next 4 lines, which are password, name, access_level, and last_logon for the user.
        for (int i = 0; i < 4; i++) {
            fgets(lineBuffer, sizeof(lineBuffer), file);
        }
    }
    fclose(file);
    return false;
}

void getCurrentDateTime(char* date, char* curr_time) {
    time_t now;
    struct tm newtime;
    
    time(&now);
    newtime = *localtime(&now);
    
    strftime(date, 15, "%d-%m-%Y", &newtime);
    strftime(curr_time, 10, "%H:%M:%S", &newtime);
}

static void strip_mail_newline(char *str) {
    if (str == NULL) {
        return;
    }
    str[strcspn(str, "\r\n")] = '\0';
}

static int read_mail_field(FILE *file, const char *prefix, char *dest, size_t dest_size) {
    char line[1024];

    if (fgets(line, sizeof(line), file) == NULL) {
        return 0;
    }

    strip_mail_newline(line);

    size_t prefix_len = strlen(prefix);
    if (strncmp(line, prefix, prefix_len) != 0) {
        return 0;
    }

    snprintf(dest, dest_size, "%s", line + prefix_len);
    return 1;
}

static int readMailRecord(FILE *file, Mail *mail) {
    char separator[32];

    if (!read_mail_field(file, "SENDER: ", mail->sender, sizeof(mail->sender))) {
        return 0;
    }
    if (!read_mail_field(file, "RECIPIENT: ", mail->recipient, sizeof(mail->recipient))) {
        return 0;
    }
    if (!read_mail_field(file, "SUBJECT: ", mail->subject, sizeof(mail->subject))) {
        return 0;
    }
    if (!read_mail_field(file, "MESSAGE: ", mail->message, sizeof(mail->message))) {
        return 0;
    }
    if (!read_mail_field(file, "DATE: ", mail->date, sizeof(mail->date))) {
        return 0;
    }
    if (!read_mail_field(file, "TIME: ", mail->time, sizeof(mail->time))) {
        return 0;
    }

    // Optional separator line between records.
    long pos = ftell(file);
    if (fgets(separator, sizeof(separator), file) != NULL) {
        strip_mail_newline(separator);
        if (strcmp(separator, "---") != 0 && separator[0] != '\0') {
            fseek(file, pos, SEEK_SET);
        }
    }

    return 1;
}

static int writeMailRecord(FILE *file, const Mail *mail) {
    if (file == NULL || mail == NULL) {
        return 0;
    }

    return fprintf(file,
                   "SENDER: %s\n"
                   "RECIPIENT: %s\n"
                   "SUBJECT: %s\n"
                   "MESSAGE: %s\n"
                   "DATE: %s\n"
                   "TIME: %s\n"
                   "---\n",
                   mail->sender,
                   mail->recipient,
                   mail->subject,
                   mail->message,
                   mail->date,
                   mail->time) > 0;
}

void addMail(Mail mail) {
    getCurrentDateTime(mail.date, mail.time);

    FILE *file = fopen("mail.txt", "a");
    if (file == NULL) {
        delayed_print("ERROR OPENING MAIL FILE\n");
        return;
    }

    if (!writeMailRecord(file, &mail)) {
        delayed_print("ERROR WRITING MAIL FILE\n");
    }

    fclose(file);
}

// For the deleteAll function:
void deleteAll(const char* username, int mode) {
    FILE *file = fopen("mail.txt", "r");
    if (file == NULL) {
        // No mail file yet simply means there is nothing to delete.
        return;
    }

    FILE *tempFile = fopen("tempMail.txt", "w");
    if (tempFile == NULL) {
        fclose(file);
        delayed_print("ERROR OPENING MAIL FILE\n");
        return;
    }

    Mail mail;
    while (readMailRecord(file, &mail)) {
        int keep = 1;

        if (mode == 0 && strcmp(mail.recipient, username) == 0) {
            keep = 0;
        } else if (mode == 1 && strcmp(mail.sender, username) == 0) {
            keep = 0;
        }

        if (keep) {
            writeMailRecord(tempFile, &mail);
        }
    }

    fclose(file);
    fclose(tempFile);

    remove("mail.txt");
    rename("tempMail.txt", "mail.txt");
}

void emailFunction(User logged_on_user) {
    char choiceBuffer[10];
    int choice;
    char command[200];
    char buffer[500]; // Buffer to hold formatted output

    do {
        clear_screen();
        play_sample_async("./samples/computer-beeps.wav");
        delayed_print("WOPR EMAIL SYSTEM\n\n");
        delayed_print("1. CREATE\n2. INBOX\n3. SENT ITEMS\n4. HOUSEKEEPING\n5. EXIT\n\nSELECT OPTION: ");
        
        fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
        choice = atoi(choiceBuffer);

        FILE *file;
        Mail mail;
        Mail mails[100];
        int mailCount = 0;

        switch (choice) {
            case 1:
                delayed_print("RECIPIENT: ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(mail.recipient, sizeof(mail.recipient), stdin);
                mail.recipient[strcspn(mail.recipient, "\n")] = 0;

                if (!userExists(mail.recipient)) {
                    delayed_print("USER DOES NOT EXIST!\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    usleep(1000000);
                    continue;
                }

                strcpy(mail.sender, logged_on_user.username);
                delayed_print("SUBJECT: ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(mail.subject, sizeof(mail.subject), stdin);
                mail.subject[strcspn(mail.subject, "\n")] = 0;

                delayed_print("MESSAGE: ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(mail.message, sizeof(mail.message), stdin);
                mail.message[strcspn(mail.message, "\n")] = 0;

                addMail(mail);
                delayed_print("EMAIL SENT!\n");
                play_sample_async("./samples/computer-beeps-short.wav");
                usleep(1000000);
                continue;

            case 2: // INBOX
                file = fopen("mail.txt", "r");
                if (file != NULL) {
                    while (mailCount < 100 && readMailRecord(file, &mail)) {
                        if (strcmp(mail.recipient, logged_on_user.username) == 0) {
                            mails[mailCount++] = mail;
                        }
                    }
                    fclose(file);
                }

                if (mailCount == 0) {
                    printf("YOU HAVE NO MAIL\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    usleep(1000000);
                    continue;
                }

                int keepCheckingMailsInbox = 1; // Flag

                while (keepCheckingMailsInbox) {
                    sprintf(buffer, "\n%-4s %-20s %-30s %-12s %-10s\n", "No.", "From", "Subject", "Date", "Time");
                    delayed_print(buffer);
                    for (int i = 0; i < mailCount; i++) {
                        printf("%-4d %-20s %-30s %-12s %-10s\n", i + 1, mails[i].sender, mails[i].subject, mails[i].date, mails[i].time);
                    }

                    delayed_print("\nSELECT EMAIL NUMBER (0 = MENU): ");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
                    int mailChoice = atoi(choiceBuffer);

                    if (mailChoice == 0) {
                        keepCheckingMailsInbox = 0;
                        continue;
                    }

                    if (mailChoice > 0 && mailChoice <= mailCount) {
                        printf("\nFROM: %s\nDATE: %s\nTIME: %s\nSUBJECT: %s\nMESSAGE: %s\n\n", 
                            mails[mailChoice-1].sender, 
                            mails[mailChoice-1].date, 
                            mails[mailChoice-1].time, 
                            mails[mailChoice-1].subject, 
                            mails[mailChoice-1].message);
            
                        delayed_print("1. REPLY TO EMAIL\n2. RETURN TO LIST\n\nSELECT OPTION: ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
                        int replyChoice = atoi(choiceBuffer);

                    if (replyChoice == 1) {
                        strcpy(mail.recipient, mails[mailChoice-1].sender);
                        strcpy(mail.sender, logged_on_user.username);
    
                        printf("RE: %s\n", mails[mailChoice-1].subject);
                        snprintf(mail.subject, sizeof(mail.subject),
                                 "RE: %.95s", mails[mailChoice-1].subject);

                        delayed_print("MESSAGE: ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(mail.message, sizeof(mail.message), stdin);
                        mail.message[strcspn(mail.message, "\n")] = 0;

                        addMail(mail);
                        delayed_print("EMAIL SENT!\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        usleep(1000000);
                    
                    } else if (replyChoice == 2) {
                            continue;
                        } else {
                            delayed_print("INVALID CHOICE. PLEASE TRY AGAIN.\n");
                            play_sample_async("./samples/computer-beeps-short.wav");
                        }
                    } else {
                        delayed_print("INVALID EMAIL NUMBER. PLEASE TRY AGAIN.\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                    }
                }
                continue;

            case 3: // SENT ITEMS
                file = fopen("mail.txt", "r");
                if (file != NULL) {
                    while (mailCount < 100 && readMailRecord(file, &mail)) {
                        if (strcmp(mail.sender, logged_on_user.username) == 0) {
                            mails[mailCount++] = mail;
                        }
                    }
                    fclose(file);
                }

                if (mailCount == 0) {
                    delayed_print("YOU HAVE NO SENT MAILS\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    usleep(1000000);
                    continue;
                }

                int keepCheckingSentMails = 1;

                while (keepCheckingSentMails) {
                    printf("\n%-4s %-20s %-30s %-12s %-10s\n", "No.", "To", "Subject", "Date", "Time");
                    for (int i = 0; i < mailCount; i++) {
                        printf("%-4d %-20s %-30s %-12s %-10s\n", i + 1, mails[i].recipient, mails[i].subject, mails[i].date, mails[i].time);
                    }

                    delayed_print("\nSELECT EMAIL NUMBER (0 = MENU): ");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
                    int mailChoice = atoi(choiceBuffer);

                    if (mailChoice == 0) {
                        keepCheckingSentMails = 0;
                        continue;
                    }

                    if (mailChoice > 0 && mailChoice <= mailCount) {
                        printf("\nTO: %s\nDATE: %s\nTIME: %s\nSUBJECT: %s\nMESSAGE: %s\n\n", 
                            mails[mailChoice-1].recipient, 
                            mails[mailChoice-1].date, 
                            mails[mailChoice-1].time, 
                            mails[mailChoice-1].subject, 
                            mails[mailChoice-1].message);
                        
                        delayed_print("1. RETURN TO LIST\n\nSELECT OPTION: ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
                        int sentChoice = atoi(choiceBuffer);
                        
                        if (sentChoice == 1) {
                            continue;
                        } else {
                            delayed_print("INVALID CHOICE. PLEASE TRY AGAIN.\n");
                            play_sample_async("./samples/computer-beeps-short.wav");
                        }
                    } else {
                        delayed_print("INVALID EMAIL NUMBER. PLEASE TRY AGAIN.\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                    }
                }
                continue;

            case 4: // HOUSEKEEPING
                clear_screen();
                delayed_print("HOUSEKEEPING\n\n");
                delayed_print("1. DELETE ALL INBOX ITEMS\n2. DELETE ALL SENT ITEMS\n3. RETURN TO MENU\n\nSELECT OPTION: ");
                play_sample_async("./samples/computer-beeps.wav");
                fgets(choiceBuffer, sizeof(choiceBuffer), stdin);
                int housekeepingChoice = atoi(choiceBuffer);

                switch (housekeepingChoice) {
                    case 1: // DELETE ALL INBOX ITEMS
                        deleteAll(logged_on_user.username, 0);
                        delayed_print("ALL INBOX ITEMS DELETED!\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        usleep(1000000);
                        break;
                    case 2: // DELETE ALL SENT ITEMS
                        deleteAll(logged_on_user.username, 1);
                        delayed_print("ALL SENT ITEMS DELETED!\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        usleep(1000000);
                        break;
                    case 3: // RETURN TO MAIN MENU
                        continue;
                    default:
                        delayed_print("INVALID CHOICE.\n");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        usleep(1000000);
                        break;
                }
                break;

            case 5:
                delayed_print("EXITING WOPR EMAIL\n");
                play_sample_async("./samples/computer-beeps-short.wav");
                usleep(1000000);
                break;

            default:
                delayed_print("INVALID CHOICE.\n");
                play_sample_async("./samples/computer-beeps-short.wav");
                usleep(1000000);
                continue;
        }
    } while(choice != 5);
}

void help_joshua() {
    char command[200];
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("\nCOMMANDS: HELP, LIST, DATE, TIME, DEFCON, AUTHOR");
    delayed_print("\n          ARPANET, INTERNET, CHAT, EXIT\n\n");
    //snprintf(command, sizeof(command), "espeak 'VALID COMMANDS: HELP, LIST, DATE, TIME, EXIT'");
    //system(command);
}

void help_user() {
    char command[200];
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("\nCOMMANDS: HELP, LIST, DATE, TIME, DEFCON, AUTHOR, USERS, MAIL, WHOAMI");
    delayed_print("\n          ARPANET, INTERNET, TIC-TAC-TOE, BACKDOOR, EXIT\n\n");
    //snprintf(command, sizeof(command), "espeak 'VALID COMMANDS: HELP, LIST, DATE, TIME, EXIT'");
    //system(command);
}

void help_games() {
    char command[200];
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("\n'GAMES' REFERS TO MODELS, SIMULATIONS, AND GAMES WHICH HAVE TACTICAL AND\nSTRATEGIC APPLICATIONS\n\n");
    //snprintf(command, sizeof(command), "espeak 'GAMES REFERS TO MODELS, SIMULATIONS, AND GAMES WHICH HAVE TACTICAL AND STRATEGIC APPLICATIONS'");
    //system(command);
}

void list_games() {
    char command[200];
    delayed_print("\nFALKEN'S MAZE\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'FALKENS MAZE'");
    //system(command);
    usleep(600000);
    delayed_print("BLACK JACK\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'BLACK JACK'");
    //system(command);
    usleep(600000);
    delayed_print("GIN RUMMY\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'GIN RUMMY'");
    //system(command);
    usleep(600000);
    delayed_print("HEARTS\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'HEARTS'");
    //system(command);
    usleep(600000);
    delayed_print("BRIDGE\n");
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'BRIDGE'");
    //system(command);
    usleep(600000);
    delayed_print("CHECKERS\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'CHECKERS'");
    //system(command);
    usleep(600000);
    delayed_print("CHESS\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'CHESS'");
    //system(command);
    usleep(600000);
    delayed_print("POKER\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'POKER'");
    //system(command);
    usleep(600000);
    delayed_print("FIGHTER COMBAT\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'FIGHTER COMBAT'");
    //system(command);
    usleep(600000);
    delayed_print("GUERRILLA ENGAGEMENT\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'GUERRILLA ENGAGEMENT'");
    //system(command);    
    usleep(600000);
    delayed_print("DESERT WARFARE\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'DESERT WARFARE'");
    //system(command);    
    usleep(600000);
    delayed_print("AIR-TO-GROUND ACTIONS\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //nprintf(command, sizeof(command), "espeak 'AIR-TO-GROUND ACTIONS'");
    //system(command);    
    usleep(600000);
    delayed_print("THEATERWIDE TACTICAL WARFARE\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'THEATERWIDE TACTICAL WARFARE'");
    //system(command);
    usleep(600000);
    delayed_print("THEATERWIDE BIOTOXIC AND CHEMICAL WARFARE\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps.wav");
    //snprintf(command, sizeof(command), "espeak 'THEATERWIDE BIOTOXIC AND CHEMICAL WARFARE'");
    //system(command);    
    usleep(1500000);
    delayed_print("\nGLOBAL THERMONUCLEAR WAR\n\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    //snprintf(command, sizeof(command), "espeak 'GLOBAL THERMONUCLEAR WAR'");
    //system(command);    
    usleep(600000);
}

void defcon_status() {
    char command[200];
    delayed_print("\nDEFCON: ");
    printf("%d",defcon);
    delayed_print("\n\n");
    play_sample_async("./samples/computer-beeps-short.wav");
}

void create_root_user() {
    char* username = "root";
    char* password = "password";
    char* name = "root";
    int access_level = 9;
    char* last_logon = "Never";

    FILE* file = fopen("users.txt", "a");
    if (file) {
        // Check if user "root" exists. If true, exit the function.
        if (userExists(username)) return;
        fprintf(file, "%s\n%s\n%s\n%d\n%s\n", username, password, name, access_level, last_logon);
        fclose(file);
    } else {
        // Handle the error, e.g., print an error message
        //printf("Error opening or creating users.txt!\n");
    }
}

void manageUsers() {
    int choice;
    char inputBuffer[256];
    char command[200];
    char buffer[500]; // Buffer to hold formatted output

    while (1) {
        play_sample_async("./samples/computer-beeps.wav");
        delayed_print("\n");
        delayed_print("1. CREATE USER\n2. AMEND USER\n3. DELETE USER\n4. LIST USERS\n\nSELECT OPTION: ");
        
        fgets(inputBuffer, sizeof(inputBuffer), stdin);
        if (sscanf(inputBuffer, "%d", &choice) != 1) {
            break;  // Break out of the loop if no valid number is provided
        }

        FILE* file;
        User tempUser;
        char inputUsername[100];

        switch (choice) {
            case 1:
                file = fopen("users.txt", "a");
                if (!file) {
                    printf("Error opening or creating users.txt!\n");
                    return;
                }
                delayed_print("USERNAME           : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(tempUser.username, sizeof(tempUser.username), stdin);
                // Convert username to lowercase
                for (int i = 0; tempUser.username[i]; i++) {
                    tempUser.username[i] = tolower(tempUser.username[i]);
                }
                strtok(tempUser.username, "\n");
                
                delayed_print("PASSWORD           : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(tempUser.password, sizeof(tempUser.password), stdin);
                strtok(tempUser.password, "\n");
                
                delayed_print("NAME               : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(tempUser.name, sizeof(tempUser.name), stdin);
                strtok(tempUser.name, "\n");
                
                delayed_print("ACCESS LEVEL       : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(inputBuffer, sizeof(inputBuffer), stdin);
                sscanf(inputBuffer, "%d", &tempUser.access_level);

                strcpy(tempUser.last_logon, "Never");

                fprintf(file, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                        tempUser.access_level, tempUser.last_logon);

                fclose(file);
                delayed_print("USER ACCOUNTED CREATED.\n");
                play_sample_async("./samples/computer-beeps-short.wav");
                break;

            case 2:
                file = fopen("users.txt", "r");
                if (!file) {
                    printf("users.txt not found. Create a user first.\n");
                    return;
                }
                delayed_print("USERNAME TO AMEND  : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(inputUsername, sizeof(inputUsername), stdin);
                strtok(inputUsername, "\n");

                FILE* tempFile = fopen("temp.txt", "w");
                if (!tempFile) {
                    printf("Error creating temp file!\n");
                    fclose(file);
                    return;
                }
                
                int amended = 0;
                while (fscanf(file, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                        &tempUser.access_level, tempUser.last_logon) != EOF) {
                    if (strcmp(tempUser.username, inputUsername) == 0) {
                        delayed_print("NEW PASSWORD       : ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(tempUser.password, sizeof(tempUser.password), stdin);
                        strtok(tempUser.password, "\n");
                        
                        delayed_print("NEW NAME           : ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(tempUser.name, sizeof(tempUser.name), stdin);
                        strtok(tempUser.name, "\n");
                        
                        delayed_print("NEW ACCESS LEVEL   : ");
                        play_sample_async("./samples/computer-beeps-short.wav");
                        fgets(inputBuffer, sizeof(inputBuffer), stdin);
                        sscanf(inputBuffer, "%d", &tempUser.access_level);

                        amended = 1;
                    }
                    fprintf(tempFile, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                            tempUser.access_level, tempUser.last_logon);
                }

                fclose(file);
                fclose(tempFile);
                remove("users.txt");
                rename("temp.txt", "users.txt");

                if (amended) {
                    delayed_print("USER ACCOUNT AMENDED.\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                } else {
                    printf("User not found.\n");
                }
                break;

            case 3:
                file = fopen("users.txt", "r");
                if (!file) {
                    printf("users.txt not found. Create a user first.\n");
                    return;
                }
                delayed_print("USERNAME TO DELETE : ");
                play_sample_async("./samples/computer-beeps-short.wav");
                fgets(inputUsername, sizeof(inputUsername), stdin);
                strtok(inputUsername, "\n");

                //check whether user to delete is root
                if (strcmp(inputUsername, "root") == 0) {
                    delayed_print("ACCESS DENIED\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                    break;
                }

                FILE* delFile = fopen("delete.txt", "w");
                if (!delFile) {
                    printf("Error creating delete file!\n");
                    fclose(file);
                    return;
                }

                int deleted = 0;
                while (fscanf(file, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                        &tempUser.access_level, tempUser.last_logon) != EOF) {
                    if (strcmp(tempUser.username, inputUsername) != 0) {
                        fprintf(delFile, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                                tempUser.access_level, tempUser.last_logon);
                    } else {
                        deleted = 1;
                    }
                }

                fclose(file);
                fclose(delFile);
                remove("users.txt");
                if (deleted) {
                    rename("delete.txt", "users.txt");
                    delayed_print("USER ACCOUNT DELETED.\n");
                    play_sample_async("./samples/computer-beeps-short.wav");
                } else {
                    remove("delete.txt");
                    printf("User not found.\n");
                }
                break;

            case 4:
                file = fopen("users.txt", "r");
                if (!file) {
                    printf("users.txt not found. Create a user first.\n");
                    return;
                }
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nUSERS:\n");
                delayed_print("-------------------------------------------------\n");
                sprintf(buffer, "| %-10s | %-15s | %-15s |\n", "Username", "Name", "Access Level");
                delayed_print(buffer);
                delayed_print("-------------------------------------------------\n");
                while (fscanf(file, "%s\n%s\n%s\n%d\n%s\n", tempUser.username, tempUser.password, tempUser.name, 
                        &tempUser.access_level, tempUser.last_logon) != EOF) {
                    printf("| %-10s | %-15s | %-15d |\n", tempUser.username, tempUser.name, tempUser.access_level);
                }
                delayed_print("-------------------------------------------------\n");
                fclose(file);
                break;

            default:
                printf("Invalid choice. Exiting...\n");
                return;  // Exit the function
        }
    }
    printf("\n");
}

void getPassword(char* password, size_t size) {
    struct termios old, new;
    int n = 0;
    char ch;

    // Disable buffering for terminal I/O so the PASS key is available.
    setvbuf(stdin, NULL, _IONBF, 0); 

    // Disable echo
    tcgetattr(fileno(stdin), &old);
    new = old;
    new.c_lflag &= ~(ECHO | ICANON);  // Disable echo and buffered input
    new.c_lflag |= ECHONL;

    if (tcsetattr(fileno(stdin), TCSAFLUSH, &new) != 0) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    printf("PASSWORD: ");
    
    // Read characters one by one, mask them with '*', and store in password array
    while (n < size - 1) {
        ch = getchar();
        if(ch == '\n')
            break;
        putchar('*');
        password[n] = ch;
        n++;
    }
    password[n] = '\0';  // Null terminate the string

    // Restore terminal
    (void) tcsetattr(fileno(stdin), TCSAFLUSH, &old);
}

void guesscode() {
    char LC[] = "CPE1704TKS";
    int LC_percent = 0;
    char buffer[200];
    char command[200];
    char input[100];

    srand(time(0)); // Initialize random seed

    int row = 10; // Desired row position
    int col = 32; // Desired column position

    clear_screen();
    delayed_print("\033[7mTERMINAL ECHO: WAR ROOM\033[0m\n");

    for (int A = 1; A <= strlen(LC); A++) {
        int LCG;
        do {
            printf("\033[%d;%dH", row, col);
            for (int B = 1; B <= LC_percent; B++) printf("%c ", LC[B - 1]);
            for (int B = 1; B <= strlen(LC) - LC_percent; B++) printf("- ");
            printf("\n");

            LCG = (rand() % (90 - 48 + 1)) + 48;
            if (LCG > 57 && LCG < 65) continue;

            printf("\033[%d;%dH", row, col);
            for (int B = 1; B <= LC_percent; B++) printf("%c ", LC[B - 1]);
            printf("%c ", (char)LCG);
                        for (int B = 1; B <= strlen(LC) - LC_percent - 1; B++) printf("- ");
            printf("\n");

            usleep(250 * 1000); // delay
        } while ((char)LCG != LC[A - 1]);
        play_sample_async("./samples/number-locked-in.wav");
        LC_percent++;
    }
    usleep(2000000);
    clear_screen();
    delayed_print("\033[7mTERMINAL ECHO: WAR ROOM\033[0m\n");
    sprintf(buffer, "\033[%d;%dH%s", 10, 32, "\033[5mC P E 1 7 0 4 T K S\033[0m");        
    delayed_print(buffer);
    usleep(10000000);
    sprintf(buffer, "\033[%d;%dH%s", 23, 28, "PRESS ENTER KEY TO CONTINUE\n");
    delayed_print(buffer);

    while(1) {        
        char selection[3]; // to accommodate the character, the '\n', and the null-terminating character
        fgets(selection, sizeof(selection), stdin); // Read user's selection

        // If user just pressed Enter, break the outer loop
        if(selection[0] == '\n' && selection[1] == '\0') {
            break;
        }
    }
    clear_screen();
    snprintf(command, sizeof(command), "./tic-tac-toe");
    int status = system(command); // Only call system(command) once
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status == 1) {
            //printf("The tic-tac-toe program exited with status 1\n");
            clear_screen();
            fflush(stdout); // flush the output buffer
            usleep(10000000);
            delayed_print("GREETINGS PROFESSOR FALKEN\n\n");
            play_sample("./samples/greetings.wav");
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/falken_hello-joshua.wav");
            } 
            fgets(input, sizeof(input), stdin);
            // Remove trailing newline character
            input[strcspn(input, "\n")] = '\0';
            // Convert input to lowercase
            for (int i = 0; input[i]; i++) {
            input[i] = tolower(input[i]);
            }
            //optionally, do something with the input - but not necessary
            usleep(500000);
            delayed_print("\nA STRANGE GAME. ");
            play_sample("./samples/a-strange-game.wav");
            usleep(500000);
            delayed_print("THE ONLY WINNING MOVE IS NOT TO PLAY!\n\n");
            play_sample("./samples/the-only-winning-move-is-not-to-play.wav");
            usleep(3000000);
            delayed_print("HOW ABOUT A NICE GAME OF CHESS?\n\n");
            play_sample("./samples/how-about-a-nice-game-of-chess.wav");
            defcon=5;
            game_running = 0;
        } else {
            //printf("The tic-tac-toe program exited with status %d\n", exit_status);
            clear_screen();
            fflush(stdout); // flush the output buffer
            usleep(10000000);
            delayed_print("\nYOU FAILED TO PREVENT WOPR FROM LAUNCHING THE MISSILES. ");
            play_sample("./samples/computer-beeps.wav");
            usleep(500000);
            delayed_print("WWIII HAS COMMENCED!\n\n");
            play_sample("./samples/computer-beeps-short.wav");
            usleep(5000000);
            defcon=1;
        }
    }
}

void map() {
    char command[200];
    play_sample_async("./samples/computer-beeps.wav");
    
    clear_screen();

    delayed_print("\n");
    delayed_print("     ____________/\\'--\\__         __                       ___/-\\             \n");
    delayed_print("   _/                   \\     __/  |          _     ___--/      / __          \n");
    delayed_print("  /                      |   /    /          / \\__--           /_/  \\/---\\    \n");
    delayed_print("  |                       \\_/    /           \\                            \\   \n");
    delayed_print("  |'                            /             |                            |  \n");
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("   \\                           |            /^                             /  \n");
    delayed_print("    \\__                       /            |                          /---/   \n");
    delayed_print("       \\__                   /              \\              ___    __  \\       \n");
    delayed_print("          \\__     ___    ___ \\               \\_           /   \\__/  /_/       \n");
    delayed_print("              \\  /    \\_/   \\ \\                \\__'-\\    /                    \n");
    delayed_print("               \\/            \\/                      \\__/                     \n");
    delayed_print("\n");

    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("          UNITED STATES                               SOVIET UNION\n\n");

}

void end_game() {
    int gte = 50;
    int etr = 10;
    char buffer[200];
    char command[200];

    for(int gc = 1; gc <= 10; gc++) {
        gte += 1;
        etr -= 1;

        if(gte == 60) {
            gte=0;
        }

        clear_screen();

        sprintf(buffer, "\033[%d;%dH%s", 19, 1, "--------------------------------------------------------------------------------");
        delayed_print(buffer);
        sprintf(buffer, "\033[%d;%dH%s", 20, 1, "GAME TIME ELAPSED");
        delayed_print(buffer);
        sprintf(buffer, "\033[%d;%dH%s", 20, 56, "ESTIMATED TIME REMAINING");
        delayed_print(buffer);

        if(gte >0) {
            sprintf(buffer, "\033[%d;%dH%s", 21, 1, "01 HRS 59 MIN");

        } else {
            sprintf(buffer, "\033[%d;%dH%s", 21, 1, "02 HRS 00 MIN");
        }
        delayed_print(buffer);
        printf(" SEC %02d", gte);

        sprintf(buffer, "\033[%d;%dH%s", 21, 56, "28 HRS 00 MIN");        
        delayed_print(buffer);
        printf(" SEC %02d", etr);

        sprintf(buffer, "\033[%d;%dH%s", 22, 1, "--------------------------------------------------------------------------------");
        delayed_print(buffer);

        play_sample_async("./samples/estimated-time-remaining.wav");

        usleep(1000000);
    }
    usleep(2000000);
    clear_screen();
    delayed_print("\033[7mTERMINAL ECHO: WAR ROOM\033[0m\n");
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("TRZ. 34/53/76               SYS PROC 3435.45.6456           XCOMP STATUS: PV-456\n");
    delayed_print("ACTIVE PORTS: 34,53,75,94                                     CPU TM USED: 23:43\n");
    delayed_print("#45/34/53.           ALT MODE FUNCT: PV-8-AY345              STANDBY MODE ACTIVE\n");
    delayed_print("#543.654      #989.283       #028.392       #099.293      #934.905      #261.372\n");
    delayed_print("\n");

    delayed_print("                         MISSILES TARGETED AND READY\n");
    delayed_print("                         ---------------------------\n\n");
    play_sample_async("./samples/computer-beeps.wav");
    usleep(1000000);
    delayed_print("\033[5m                             CHANGES LOCKED OUT\033[0m\n");
    delayed_print("                             ------------------\n"); 
    play_sample_async("./samples/buzzer-sounds.wav");
    usleep(3000000);

    defcon=1;

    usleep(5000000);
    clear_screen();
    delayed_print("\033[7mTERMINAL ECHO: WAR ROOM\033[0m\n");
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("TRZ. 34/53/76               SYS PROC 3435.45.6456           XCOMP STATUS: PV-456\n");
    delayed_print("ACTIVE PORTS: 34,53,75,94                                     CPU TM USED: 23:43\n");
    delayed_print("#45/34/53.           ALT MODE FUNCT: PV-8-AY345              STANDBY MODE ACTIVE\n");
    delayed_print("#543.654      #989.283       #028.392       #099.293      #934.905      #261.372\n");
    delayed_print("\n");

    delayed_print("                            PRIMARY TARGET IMPACT\n");
    delayed_print("                            ---------------------\n\n");
    play_sample_async("./samples/computer-beeps.wav");
    usleep(1000000);

    delayed_print("                    LORING AIRFORCE BASE      : ");
    play_sample_async("./samples/computer-beeps.wav");
    usleep(2000000);
    delayed_print("NO IMPACT\n");
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(2000000);

    delayed_print("                    ELMENDORF AIRFORCE BASE   : ");
    play_sample_async("./samples/computer-beeps.wav");
    usleep(2000000);
    delayed_print("NO IMPACT\n");
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(2000000);

    delayed_print("                    GRAND FORKS AIRFORCE BASE : ");
    play_sample_async("./samples/computer-beeps.wav");
    usleep(2000000);
    delayed_print("NO IMPACT\n");
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(2000000);

    usleep(10000000);
    guesscode();
    
    //rest of game goes here
    //this should include: Joshua searching/finding launch codes
    //tic-tac-toe sequence

}

void global_thermonuclear_war() {
    char command[200];
    int count = 0;
    char side[20];  // Array to store the selected side
    char input;
    int col=0; //print at col
    int row=0; //print at row
    int t;
    char buffer[200];
    char* prompt = "";
    startgame:
    clear_screen();
    map();
    delayed_print("WHICH SIDE DO YOU WANT?\n\n");
    if(count == 0) {
        //snprintf(command, sizeof(command), "espeak 'WHICH SIDE DO YOU WANT?'");
        play_sample("./samples/which-side-do-you-want.wav");
        delayed_print("  1. UNITED STATES\n");
        delayed_print("  2. SOVIET UNION\n\n");
        delayed_print("PLEASE CHOOSE ONE: ");
        if(hints == 1) {
            usleep(2000000);
            play_sample("./samples/david_ill-be-the-russians.wav");
        } 
        
        scanf(" %c", &input);
        
        if (input == '1') {
            strcpy(side, "UNITED STATES");
        } else if (input == '2') {
            strcpy(side, "SOVIET UNION");
        } else {
            delayed_print("\nINVALID OPTION\n\n");
            usleep(5000000);
        }
        count=count+1;
        goto startgame;
        
    } else {
        if (input == '1') {
            delayed_print("\033[5m  1. UNITED STATES\033[0m\n");
            delayed_print("  2. SOVIET UNION\n\n");
        } else {
            delayed_print("  1. UNITED STATES\n"); 
            delayed_print("\033[5m  2. SOVIET UNION\033[0m\n\n");   
        }
    }
    
   
    clear_input_buffer();
    // Rest of the game code goes here: start
    delayed_print("YOU HAVE SELECTED: ");
    delayed_print(side);
    usleep(2500000);
    input_targets:;
    clear_screen ();
    usleep(2500000);
    delayed_print("\033[4mAWAITING FIRST STRIKE COMMAND\033[24m\n\n");
    delayed_print("PLEASE LIST PRIMARY TARGETS BY\n");
    delayed_print("CITY AND/OR COUNTY NAME:\n\n");
    //snprintf(command, sizeof(command), "espeak 'PLEASE LIST PRIMARY TARGETS'");
    play_sample("./samples/please-list-primary-targets.wav");
    
    //while loop to input targets goes here
    char targets[MAX_TARGETS][MAX_STRING_LENGTH];
    count = 0;
    while(count < MAX_TARGETS) {
        if(fgets(targets[count], MAX_STRING_LENGTH, stdin) == NULL) {
            break;
        }

        // Remove the newline character at the end of the string
        targets[count][strcspn(targets[count], "\n")] = 0;

        // Check for empty string (i.e., carriage return)
        if(strlen(targets[count]) == 0) {
            break;
        }

        count++;
    }

    //delayed_print("\n\nMAX TARGETS SELECTED");
    delayed_print("\nTARGET SELECTION COMPLETE\n\n");
    play_sample_async("./samples/computer-beeps.wav");
    //snprintf(command, sizeof(command), "espeak 'TARGET SELECTION COMPLETE'");
    //system(command);
    usleep(2500000);

    while(1) {
        map();
        delayed_print("\033[4mPRIMARY TARGETS\033[24m\n");
        for (int i = 0; i < count; i++) {
            for (int j = 0; targets[i][j] != '\0'; j++) {
                putchar(toupper(targets[i][j]));
                usleep(CHARACTER_DELAY);
            }
            usleep(1000000);
            play_sample_async("./samples/computer-beeps-short.wav");
            printf("\n");
        }
        usleep(1000000);
        play_sample_async("./samples/computer-beeps-short.wav");
        delayed_print("\nCOMMAND (L = LAUNCH, E = EDIT TARGETS, X = EXIT): ");
        scanf(" %c", &input);
        clear_input_buffer();
        if (input == 'l' || input == 'L') {
            usleep(2000000);
            defcon=3;
            break;
        } else if (input == 'e' || input == 'E') {
            usleep(2000000);
            defcon=5;
            goto input_targets;
        } else if (input == 'x' || input == 'X') {
            usleep(2000000);
            defcon=5;
            clear_screen();
            goto end_missile_launch;
        }
    }
       
    map();
    usleep(1000000);
    delayed_print("\033[4mTRAJECTORY HEADING\033[24m");
    delayed_print("   ");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(500000);
    delayed_print("\033[4mTRAJECTORY HEADING\033[24m");
    delayed_print("  ");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(500000);
    delayed_print("\033[4mTRAJECTORY HEADING\033[24m");
    delayed_print("   ");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(500000);
    delayed_print("\033[4mTRAJECTORY HEADING\033[24m");
    delayed_print("\n");
    fflush(stdout); // flush the output buffer
    play_sample_async("./samples/computer-beeps-short.wav");
    usleep(2000000);
   
    for (int t = 0; t < count; t++) {
        if(t == 0) {
            sprintf(buffer, "\033[%d;%dH%s", 17, 1, "A-5520-A 939 523");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 10, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }
            
            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 62, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 18, 1, "       B 664 295");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 11, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 62, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            } 

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

          
            sprintf(buffer, "\033[%d;%dH%s", 19, 1, "       C 125 386");
            delayed_print(buffer);
            
            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 12, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }
            
            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 61, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }    

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 20, 1, "       D 768 347");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 13, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 60, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 21, 1, "       E 463 284");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 14, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }


            if (strstr(side, "SOVIET UNION") != NULL) {
                delayed_print(buffer);
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 59, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);
        }

        if(t == 1) {
            sprintf(buffer, "\033[%d;%dH%s", 17, 22, "B-5520-A 243 587");
            delayed_print(buffer);
            
            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 10+5, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 62-5, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }
            
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 18, 22, "       B 892 754");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 11+5, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 62-5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 19, 22, "       C 374 256");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 12+5, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 61-5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 20, 22, "       D 364 867");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 13+5, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 60-5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 21, 22, "       E 463 284");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 14+5, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 59-5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);
        }


        if(t == 2) {
            sprintf(buffer, "\033[%d;%dH%s", 17, 42, "C-5520-A 398 984");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 10+10, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 62-10, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 18, 42, "       B 394 345");
            delayed_print(buffer);
            
            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 11+10, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 62-10, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 19, 42, "       C 407 340");
            delayed_print(buffer);


            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 12+10, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 61-10, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 20, 42, "       D 251 953");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 13+10, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }


            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 60-10, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }


            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 21, 42, "       E 093 684");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 14+10, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 59-10, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);
        }

        if(t == 3) {
            sprintf(buffer, "\033[%d;%dH%s", 17, 63, "D-5520-A 919 437");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 10+15, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }
            
            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 8, 62+5, "/\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 18, 63, "       B 132 147");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 11+15, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 7, 62+5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 19, 63, "       C 095 485");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 12+15, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 6, 61+5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 20, 63, "       D 489 794");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 13+15, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
             printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 5, 60+5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);

            sprintf(buffer, "\033[%d;%dH%s", 21, 63, "       E 025 344");
            delayed_print(buffer);

            if (strstr(side, "UNITED STATES") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 14+15, "/");
                delayed_print(buffer);
                printf("\033[0m");
            }

            if (strstr(side, "SOVIET UNION") != NULL) {
                printf("\033[31m");
                sprintf(buffer, "\033[%d;%dH%s", 4, 59+5, "\\");
                delayed_print(buffer);
                printf("\033[0m");
            }

            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("   ");
            fflush(stdout); // flush the output buffer
            usleep(1000000);
        }
    } 
    usleep(5000000);

    sprintf(buffer, "\033[%d;%dH%s", 23, 28, "PRESS ENTER KEY TO CONTINUE\n");
    delayed_print(buffer);

    while(1) {        
        char selection[3]; // to accommodate the character, the '\n', and the null-terminating character
        fgets(selection, sizeof(selection), stdin); // Read user's selection

        // If user just pressed Enter, break the outer loop
        if(selection[0] == '\n' && selection[1] == '\0') {
            break;
        }
    }
    
    game_running = 1;
    clear_screen();
    usleep(1000000);
    play_sample("./samples/disconnected-2x.wav");
    not_delayed_print("MODEM CARRIER LOST\n");
    not_delayed_print("--DISCONNECTED--");
    fflush(stdout); // flush the output buffer
    usleep(5000000);
    clear_screen();
    not_delayed_print("INCOMING MODEM CARRIER\n");
    fflush(stdout); // flush the output buffer
    play_sample("./samples/telephone-ring-short.wav");
    usleep(2000000);
    clear_screen();
    not_delayed_print("CONNECTING");
    fflush(stdout); // flush the output buffer
    play_sample("./samples/1200-modem.wav");
    usleep(2000000);
    clear_screen();
    fflush(stdout); // flush the output buffer
    usleep(3000000);
    delayed_print("GREETINGS PROFESSOR FALKEN.\n\n");
    //snprintf(command, sizeof(command), "espeak 'GREETINGS PROFESSOR FALKEN'");
    play_sample("./samples/greetings.wav");
    delayed_print("TIP: TYPE 'HELP' FOR SYSTEM COMMANDS. TYPE 'CHAT' FOR JOSHUA CHAT.\n\n");
    delayed_print(prompt);
    if(hints == 1) {
        usleep(2000000);
        play_sample("./samples/david_incorrect-identification.wav");
        usleep(500000);
        play_sample("./samples/david_i-am-not-falken-falken-is-dead.wav");
    } 
    usleep(2000000);
    //control returned to joshua function
    end_missile_launch:;
}

void joshua() {
    char command[200];

    // Warm the local neural response module in the background while Joshua's
    // normal scripted startup sequence is displayed.
    start_wopr_chat_service();

    clear_screen();
    char* prompt = "";
    int i;
    for (i = 0; i < 3; i++) {
    play_sample_async("./samples/computer-beeps.wav");
    not_delayed_print("145          11456          11889          11893                                \n");
    not_delayed_print("PRT CON. 3.4.5. SECTRAN 9.4.3.          PORT STAT: SB-345                      \n");
    not_delayed_print("                                                                                \n");
    clear_screen ();
    not_delayed_print("(311) 655-7385                                                                 \n");
    not_delayed_print("                                                                                \n");
    not_delayed_print("                                                                                \n");
    clear_screen ();
    not_delayed_print("(311) 767-8739                                                                 \n");
    not_delayed_print("(311) 936-2364                                                                 \n");
    clear_screen();
    not_delayed_print("\033[7mPRT. STAT.                   CRT. DEF.                                    \033[0m\n");
    not_delayed_print("================================================                            \n");
    not_delayed_print("\033[7mFSKJJSJ: SUSJKJ: SUFJSL:          DKSJL: SKFJJ: SDKFJLJ                   \033[0m\n");
    not_delayed_print("\033[7mSYSPROC FUNCT READY          ALT NET READY                                \033[0m\n");
    not_delayed_print("\033[7mCPU AUTH RY-345-AX3     SYSCOMP STATUS: ALL PORTS ACTIVE                  \033[0m\n");
    not_delayed_print("22/34534.90/3289               CVB-3904-39490                              \n");
    not_delayed_print("(311) 936-2384                                                                 \n");
    not_delayed_print("(311) 936-3582                                                                 \n");
    clear_screen();
    not_delayed_print("22/34534.3209                  CVB-3904-39490                              \n");
    not_delayed_print("12934-AD-43KJ: CENTR PAK                                                      \n");
    not_delayed_print("(311) 767-1083                                                                 \n");
    clear_screen();
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\033[7mFLD CRS: 33.34.543     HPBS: 34/56/67/83     STATUS FLT  034/304          \033[0m\n");
    not_delayed_print("\033[7m1105-45-F6-B456          NOPR STATUS: TRAK OFF     PRON ACTIVE            \033[0m\n");
    not_delayed_print("(45:45:45  WER: 45/29/01 XCOMP: 43239582 YCOMP: 3492930D ZCOMP: 343906834        \n");
    not_delayed_print("                                          SRON: 65=65/74/84/65/89            \n");
    not_delayed_print("\033[2J\033[H                                                                 \n");
    not_delayed_print("-           PRT. STAT.                        CRY. DEF.                      \n");
    not_delayed_print("(311) 936-1582==============================================                \n");
    not_delayed_print("                  3453                3594                                   \n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("\n");
    not_delayed_print("FLJ42   TK01   BM90   R601   6J82   FP03   ZWO::   JW89                       \n");
    not_delayed_print("DEF TRPCON: 43.45342.349                                                      \n");
    not_delayed_print("\033[7mCPU AUTH RY-345-AX3     SYSCOMP STATUS: ALL PORTS ACTIVE                  \033[0m\n");
    not_delayed_print("(311) 936-2364                                                                 \n");
    not_delayed_print("**********************************************************************        \n");
    not_delayed_print("1105-45-F6-B456                 NOPR STATUS: TRAK OFF   PRON ACTIVE          \n");
    not_delayed_print("\033[2J\033[H                                                                 \n");
    }

    usleep(5000000);
    delayed_print("GREETINGS PROFESSOR FALKEN.\n\n");
    //snprintf(command, sizeof(command), "espeak 'GREETINGS PROFESSOR FALKEN'");
    play_sample("./samples/greetings.wav");
    delayed_print(prompt);
    if(hints == 1) {
        usleep(2000000);
        play_sample("./samples/david_hello.wav");
    }  
    char input[8192];
    int woprchat = 0;
    int whatcount = 0;
    int chat_mode = 0;  // Default is the original deterministic simulator.
    while (1) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // Keep one terminal submission as one turn. If the line is longer than
        // the Joshua input buffer, discard the remainder instead of allowing it
        // to become a second, accidental message on the next loop iteration.
        size_t input_len = strlen(input);
        if (input_len > 0 && input[input_len - 1] == '\n') {
            input[input_len - 1] = '\0';
        } else if (!feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {
                // Drain the rest of this overlong line.
            }
        }

        // Also tolerate CRLF input.
        input[strcspn(input, "\r")] = '\0';

        // Convert input to lowercase
        for (int i = 0; input[i]; i++) {
            input[i] = tolower(input[i]);
        }

        char wopr_action[64] = "NONE";

        if (is_joshua_chat_command(input)) {
            if (wopr_chat_enabled == 1) {
                chat_mode = 1;
                delayed_print("\nJOSHUA CHAT ACTIVE. TYPE 'RESUME' TO RETURN TO SIMULATION.\n\n");
            } else {
                delayed_print("\nJOSHUA CHAT IS DISABLED.\n\n");
            }
            delayed_print(prompt);
            continue;
        }

        if (chat_mode && is_joshua_resume_command(input)) {
            chat_mode = 0;
            delayed_print("\nRETURNING TO SIMULATION.\n\n");
            delayed_print(prompt);
            continue;
        }

        if (chat_mode && wopr_chat_enabled == 1 && !is_joshua_local_command(input)) {
            // In chat mode, Joshua may converse freely. Any control metadata
            // is stripped by wopr_chat_response() but deliberately ignored.
            wopr_chat_response(input, wopr_action, sizeof(wopr_action));
            delayed_print(prompt);
            continue;
        }

        if (strcmp(input, "help") == 0) {
            help_joshua();
            delayed_print(prompt);     
        } else if (strcmp(input, "help games") == 0) {
            help_games();
            delayed_print(prompt);
        } else if (strcmp(input, "") == 0) {
            //delayed_print("\n\n");
            delayed_print(prompt);
        } else if (strcmp(input, "list") == 0) {
            show_list();
            //snprintf(command, sizeof(command), "espeak 'USE SYNTAX: LIST TYPE'");
            //system(command);
            delayed_print(prompt);
        } else if (strcmp(input, "internet") == 0) {
            connect_internet();
            //snprintf(command, sizeof(command), "espeak 'USE SYNTAX: LIST TYPE'");
            //system(command);
            delayed_print(prompt);
        } else if (strcmp(input, "arpanet") == 0) {
            connect_arpanet();
            //snprintf(command, sizeof(command), "espeak 'USE SYNTAX: LIST TYPE'");
            //system(command);
            delayed_print(prompt);
        } else if (strcmp(input, "list games") == 0) {
            list_games();
            delayed_print(prompt);
        } else if (strcmp(input, "global thermonuclear war") == 0 && woprchat != 3 && woprchat != 4 && game_running == 0) {
            global_thermonuclear_war();
            delayed_print(prompt);
        } else if (strcmp(input, "global thermonuclear war") == 0 && game_running == 1) {
            delayed_print("\nGAME ROUTINE RUNNING\n\n");
            play_sample("./samples/computer-beeps-short.wav");
            delayed_print(prompt);
        } else if (strcmp(input, "date") == 0) {
            show_date();
            delayed_print(prompt);
        } else if (strcmp(input, "time") == 0) {
            show_time();
            delayed_print(prompt);
        } else if ((strstr(input, "hello") != NULL || strstr(input, "hi") != NULL) && woprchat == 0 && game_running == 0) {
            delayed_print("\nHOW ARE YOU FEELING TODAY?\n\n");
            //snprintf(command, sizeof(command), "espeak 'HOW ARE YOU FEELING TODAY?'");
            play_sample("./samples/computer-beeps.wav");
            delayed_print(prompt);
            woprchat = 1;
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_im-fine-how-are-you.wav");
            }           
        } else if ((strstr(input, "fine") != NULL || strstr(input, "good") != NULL || strstr(input, "well") != NULL) && woprchat == 1 && game_running == 0) {
            delayed_print("\nEXCELLENT. ");
            play_sample("./samples/excellent.wav");
            delayed_print("IT'S BEEN A LONG TIME. ");
            play_sample("./samples/its-been-long-time.wav");
            delayed_print("CAN YOU EXPLAIN THE REMOVAL OF YOUR USER\n");
            delayed_print("ACCOUNT ON 6/23/1973?\n\n");
            play_sample("./samples/can-you-explain-the-removal-of-your-user-account.wav");
            //snprintf(command, sizeof(command), "espeak 'EXCELLENT. ITS BEEN A LONG TIME. CAN YOU EXPLAIN THE REMOVAL OF YOUR USER ACCOUNT ON JUNE twenty third, nineteen seventy three'");
            delayed_print(prompt);
            woprchat = 2;
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_people-sometimes-make-mistakes.wav");
            } 
        } else if (strstr(input, "mistake") != NULL && woprchat == 2 && game_running == 0) {
            delayed_print("\nYES THEY DO. ");
            //snprintf(command, sizeof(command), "espeak 'YES THEY DO.'");
            play_sample("./samples/yes-they-do.wav");
            usleep(200000);
            delayed_print("SHALL WE PLAY A GAME?\n\n");
            //snprintf(command, sizeof(command), "espeak 'SHALL WE PLAY A GAME'");
            play_sample("./samples/play-a-game.wav");
            delayed_print(prompt);
            woprchat = 3;
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_how-about-global-thermonuclear-war.wav");
            } 
        } else if (strstr(input, "global thermonuclear war") != NULL && woprchat == 3 && game_running == 0) {
            delayed_print("\nWOULDN'T YOU PREFER A GOOD GAME OF CHESS?\n\n");
            //snprintf(command, sizeof(command), "espeak 'WOULDNT YOU PREFER A GOOD GAME OF CHESS'");
            play_sample("./samples/a-good-game-of-chess.wav");
            delayed_print(prompt);
            woprchat = 4;
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_later-lets-play-global-thermonuclear-war.wav");
            } 
        } else if ((strstr(input, "later") != NULL || strstr(input, "global thermonuclear war") != NULL || strcmp(input, "yes") == 0) && woprchat == 4 && game_running == 0) {
            delayed_print("\nFINE\n\n");
            //snprintf(command, sizeof(command), "espeak 'FINE'");
            play_sample("./samples/fine.wav");
            usleep(1000000);
            global_thermonuclear_war();
            delayed_print(prompt);
            woprchat = 5;
        } else if (strcmp(input, "exit") == 0) {
            play_sample_async("./samples/computer-beeps.wav");
            delayed_print("\nSESSION CLOSED\n--CONNECTION TERMINATED--\n");
            usleep(1000000);
            exit(0);
        } else if (strcmp(input, "author") == 0) {
            author();
        } else if ((strstr(input, "not falken") != NULL || strstr(input, "falken is dead") != NULL || strstr(input, "incorrect") != NULL) && game_running == 1) {
            delayed_print("\nI'M SORRY TO HEAR THAT, PROFESSOR.\n");
            play_sample("./samples/sorry-to-hear-that-professor.wav");
            delayed_print("\nYESTERDAY'S GAME WAS INTERRUPTED.\n");
            play_sample("./samples/yesterdays-game-was-interrupted.wav");
            delayed_print("\nALTHOUGH PRIMARY GOAL HAS NOT YET\n");
            play_sample("./samples/although-primary-goal-has-not-yet.wav");
            delayed_print("BEEN ACHIEVED, SOLUTION IS NEAR.\n\n");
            play_sample("./samples/been-achieved-solution-is-near.wav");
            usleep(1000000);
            delayed_print(prompt);
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_what-is-the-primary-goal-1st-time.wav");
            } 
        } else if (strstr(input, "primary goal") != NULL && whatcount == 0 && game_running == 1) {
            whatcount = 1;
            delayed_print("\nYOU SHOULD KNOW PROFESSOR. ");
            play_sample("./samples/you-should-know-professor.wav");
            usleep(500000);
            delayed_print("YOU PROGRAMMED ME.\n\n");
            play_sample("./samples/you-programmed-me.wav");
            usleep(1000000);
            delayed_print(prompt);
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_what-is-the-primary-goal-2nd-time.wav");
            } 
        } else if (strstr(input, "primary goal") != NULL && whatcount == 1 && game_running == 1) {
            whatcount = 2;
            delayed_print("\nTO WIN THE GAME.\n\n");
            play_sample("./samples/to-win-the-game.wav");
            usleep(1000000);
            delayed_print(prompt);
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_are-you-still-playing-the-game.wav");
            } 
        } else if ((strstr(input, "still playing") != NULL || strstr(input, "still running") != NULL) && game_running == 1) {
            whatcount = 2;
            delayed_print("\nOF COURSE. ");
            play_sample("./samples/of-course.wav");
            usleep(500000);
            delayed_print("\nI SHOULD REACH DEFCON 1 AND\nLAUNCH MY MISSILES IN 28 HOURS.\n");
            play_sample("./samples/i-should-reach-defcon-1-and-launch-my-missiles-in-28-hours.wav");
            usleep(1000000);
            delayed_print("\nWOULD YOU LIKE TO SEE SOME PROJECTED KILL RATIOS?\n\n");
            play_sample("./samples/would-you-like-to-see-some-projected-kill-ratios.wav");
            usleep(2000000);
            delayed_print("UNITED STATES                                      SOVIET UNION\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("UNITS DESTROYED          MILITARY ASSETS           UNITS DESTROYED\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("--------------------------------------------------------------------------------");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     60%                 BOMBERS                         48%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     54%                 ICBM                            51%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     12%                 ATTACK SUBS                     23%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     39%                 TACTICAL AIRCRAFT               46%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     50%                 GROUND FORCES                   52%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\n\n");
            usleep(1000000);
            delayed_print("UNITED STATES                                      SOVIET UNION\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("UNITS DESTROYED          CIVILIAN ASSETS           UNITS DESTROYED\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("--------------------------------------------------------------------------------");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     60%                 HOUSING                         56%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     22%                 COMMUNICATIONS                  37%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     45%                 TRANSPORTATION                  41%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     70%                 FOOD STOCKPILES                 82%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("     89%                 HOSPITALS                       91%\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\n\n");
            usleep(1000000);
            delayed_print("UNITED STATES            HUMAN RESOURCES           SOVIET UNION\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("--------------------------------------------------------------------------------");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("49 MILLION               NON-FATAL INJURED         51 MILLION\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("72 MILLION               POPULATION DEATHS         75 MILLION\n");
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\n");
            usleep(1000000);
            delayed_print(prompt);
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_is-this-a-game-or-is-it-real.wav");
            } 
        } else if ((strstr(input, "game or real") != NULL || strstr(input, "is this a game") != NULL || strstr(input, "is it real") != NULL) && whatcount == 2 && game_running == 1) {
            whatcount = 3;
            delayed_print("\nWHAT'S THE DIFFERENCE?\n\n");
            play_sample("./samples/whats-the-difference.wav");
            usleep(3000000);
            delayed_print("YOU ARE A HARD MAN TO REACH. ");
            play_sample("./samples/you-are-a-hard-man-to-reach.wav");
            usleep(500000);
            delayed_print("COULD NOT FIND\n");
            delayed_print("YOU IN SEATTLE AND NO TERMINAL IS IN\n");
            delayed_print("OPERATION AT YOUR CLASSIFIED ADDRESS.\n\n");
            play_sample("./samples/could-not-find-you-in-seattle-and-no-terminal-is-in-operation-at-your-classified-address.wav");
            delayed_print(prompt);
            if(hints == 1) {
                usleep(2000000);
                play_sample("./samples/david_what-classified-address.wav");
            } 
        } else if (strstr(input, "address") != NULL && whatcount == 3 && game_running == 1) {
            whatcount = 4;
            delayed_print("\nDOD PENSION FILES INDICATE\n");
            delayed_print("CURRENT MAILING AS:\n");
            play_sample("./samples/dod-pension-files-indicate-current-mailing-as.wav");
            delayed_print("DR. ROBERT HUME (A.K.A. STEPHEN W. FALKEN)\n");
            play_sample("./samples/dr-robert-hume-a-k-a-stephen-w-falken.wav");
            delayed_print("5 TALL CEDAR ROAD\n");
            delayed_print("GOOSE ISLAND, OREGON 97014\n\n");
            play_sample("./samples/5-tall-cedar-road-goose-island-oregon.wav");
            usleep(500000);
            press_enter_to_continue();
            end_game();
            delayed_print(prompt);
        } else if (strcmp(input, "defcon") == 0) {
            defcon_status();
            delayed_print(prompt);
        } else if (strcmp(input, "tic-tac-toe") == 0) {
            snprintf(command, sizeof(command), "./tic-tac-toe");
            system(command);
            delayed_print(prompt);
        } else if (strcmp(input, "cls") == 0) {
            clear_screen();
            delayed_print(prompt);
        //} else if (strcmp(input, "users") == 0) {
        //    manageUsers();
        } else {
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nUNRECOGNIZED INPUT. TYPE 'CHAT' FOR JOSHUA CHAT.\n\n");
            delayed_print(prompt);
        }
    }

    return;
}

void logged_on_user(User user) {
    char command[200];
    char buffer[500]; // Buffer to hold formatted output
    char input[100];

    clear_screen();
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("Welcome to NORAD, Cheyenne Mountain Complex\n");
    delayed_print("\033[4mW\033[0mar \033[4mO\033[0mperation \033[4mP\033[0mlan \033[4mR\033[0mesponse\n\n");
    delayed_print("NOTE: Access to this system is restricted to authorised personnel only.\n");
    delayed_print("      Unauthorised access to this system is a federal offence.\n\n");
    usleep(5000000);
    
    clear_screen();
    play_sample_async("./samples/computer-beeps.wav");
    delayed_print("TRZ. 34/53/76               SYS PROC 3435.45.6456           XCOMP STATUS: PV-456\n");
    delayed_print("ACTIVE PORTS: 34,53,75,94                                     CPU TM USED: 23:43\n");
    delayed_print("#45/34/53.           ALT MODE FUNCT: PV-8-AY345              STANDBY MODE ACTIVE\n");
    delayed_print("#543.654      #989.283       #028.392       #099.293      #934.905      #261.372\n");
    delayed_print("\n");
    delayed_print("HINT: TYPE HELP FOR A LIST OF COMMANDS\n\n");

    sprintf(buffer, "USER         : %s\n", user.username);
    delayed_print(buffer);

    sprintf(buffer, "NAME         : %s\n", user.name);
    delayed_print(buffer);
    
    sprintf(buffer, "ACCESS LEVEL : %d\n", user.access_level);
    delayed_print(buffer);

    delayed_print("\n");
    
    while(1) {
        delayed_print("> ");
        fgets(input, sizeof(input), stdin);
        
        // Remove trailing newline character
        input[strcspn(input, "\n")] = '\0';

        // If user just pressed Enter (carriage return)
        if (input[0] == '\0') {
            continue; // Go back to the start of the while loop
        }

        // Convert input to lowercase
        for (int i = 0; input[i]; i++) {
            input[i] = tolower(input[i]);
        }
        
        // Check commands
        if (strcmp(input, "help games") == 0) {
            help_games();
        }
        else if (strcmp(input, "help") == 0) {
            help_user();
        }
        else if (strcmp(input, "defcon") == 0) {
            defcon_status();
        }
        else if (strcmp(input, "list games") == 0) {
            list_games();
        }
        else if (strcmp(input, "list") == 0) {
            show_list();
        }
        else if (strcmp(input, "author") == 0) {
            author();
        }
        else if (strcmp(input, "internet") == 0) {
            if (user.access_level >= 3) {
                connect_internet();
            } else {
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nPERMISSION DENIED. ACCESS LEVEL REQUIRED: 3\n\n");
            }
        } else if (strcmp(input, "arpanet") == 0) {        
            if (user.access_level >= 3) {
                connect_arpanet();
            } else {
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nPERMISSION DENIED. ACCESS LEVEL REQUIRED: 3\n\n");
            }
        }
        else if (strcmp(input, "whoami") == 0) {
            delayed_print("\nUSER: ");
            printf("%s",user.username);
            delayed_print("\n\n");
            play_sample_async("./samples/computer-beeps-short.wav");
        }
        else if (strcmp(input, "cls") == 0) {
            clear_screen();
            play_sample_async("./samples/computer-beeps.wav");
            delayed_print("TRZ. 34/53/76               SYS PROC 3435.45.6456           XCOMP STATUS: PV-456\n");
            delayed_print("ACTIVE PORTS: 34,53,75,94                                     CPU TM USED: 23:43\n");
            delayed_print("#45/34/53.           ALT MODE FUNCT: PV-8-AY345              STANDBY MODE ACTIVE\n");
            delayed_print("#543.654      #989.283       #028.392       #099.293      #934.905      #261.372\n");
            delayed_print("\n");
            delayed_print("HINT: TYPE HELP FOR A LIST OF COMMANDS\n\n");
        }
        else if (strcmp(input, "users") == 0) {
            if (user.access_level >= 5) {
                manageUsers();
            } else {
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nPERMISSION DENIED. ACCESS LEVEL REQUIRED: 5\n\n");
            }
           
        }
        else if (strcmp(input, "backdoor") == 0) {
            if (user.access_level >= 5) {
                char buffer[10];
                int user_input;
                play_sample_async("./samples/computer-beeps-short.wav");
                delayed_print("\nSET STATUS (0 = DISABLED, 1 = ENABLED): ");
                fgets(buffer, sizeof(buffer), stdin);
                
                // Convert the string to an integer
                user_input = atoi(buffer);

                if (set_status_to_file("joshua.txt", user_input) != 0) {
                    play_sample_async("./samples/computer-beeps-short.wav");
                    delayed_print("ERROR SETTING STATUS.\n\n");
                } else {
                    play_sample_async("./samples/computer-beeps-short.wav");
                    delayed_print("STATUS UPDATED.\n\n");
                }
            } else {
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nPERMISSION DENIED. ACCESS LEVEL REQUIRED: 5\n\n");
            }
           
        }
        else if (strcmp(input, "mail") == 0) {
            emailFunction(user);
        } 
        else if (strcmp(input, "date") == 0) {
            show_date();
        } 
        else if (strcmp(input, "time") == 0) {
            show_time();
        } 
        else if (strcmp(input, "tic-tac-toe") == 0) {
            snprintf(command, sizeof(command), "./tic-tac-toe");
            system(command);
        }         
        else if (strcmp(input, "exit") == 0) {
            play_sample_async("./samples/computer-beeps.wav");
            delayed_print("\nLOGGING OUT OF SESSION\n--CONNECTION TERMINATED--\n");
            usleep(1000000);
            exit(0);
        }
        else {
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nINVALID COMMAND\n\n");
        }
    }
}

int authenticateUser(char* username) {
    char inputPassword[100];
    FILE* file = fopen("users.txt", "r");
    if (!file) {
        printf("users.txt not found. Ensure a user exists.\n");
        return 0;
    }

    User tempUser;

    getPassword(inputPassword, sizeof(inputPassword));

    int authenticated = 0;
    while (fscanf(file, "%99s\n%99s\n%99s\n%d\n%99s\n", tempUser.username, tempUser.password, tempUser.name, 
            &tempUser.access_level, tempUser.last_logon) != EOF) {
        if (strcmp(tempUser.username, username) == 0 && strcmp(tempUser.password, inputPassword) == 0) {
            authenticated = 1;
            break;
        }
    }
    fclose(file);

    if (authenticated) {
        printf("\nUSER AUTHENTICATION SUCCESSFUL\n");
        usleep(1000000);
        logged_on_user(tempUser); // Call this function if authenticated
        return 1;
    }

    return 0;
}

void handle_user_input() {
    char* prompt = "LOGON: ";
    char input[100];
    char command[200];
    int failed_attempts = 0;
    
    while (1) {
        usleep(500000);
        play_sample_async("./samples/computer-beeps-short.wav");
        delayed_print(prompt);

        fgets(input, sizeof(input), stdin);

        // Remove trailing newline character
        input[strcspn(input, "\n")] = '\0';

        // Check if user just pressed Enter (carriage return)
        if (input[0] == '\0') {
            failed_attempts++;
            play_sample_async("./samples/computer-beeps.wav");
            delayed_print("\nIDENTIFICATION NOT RECOGNIZED BY SYSTEM\n");
            if (failed_attempts >= 3) {
                delayed_print("--CONNECTION TERMINATED--\n");
                usleep(1000000);
                break;
            }
            delayed_print("\n");
            press_enter_to_continue();
            clear_screen();
            continue;
        }

        // Convert input to lowercase
        for (int i = 0; input[i]; i++) {
            input[i] = tolower(input[i]);
        }

        // Handle user input options
        if (strcmp(input, "help") == 0) {
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nNO HELP AVAILABLE\n\n");
        } else if (strcmp(input, "help logon") == 0) {
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nNO HELP AVAILABLE\n\n");
        } else if (strcmp(input, "help games") == 0) {
            help_games();
        } else if (strcmp(input, "list games") == 0) {
            list_games();
        } else if (strcmp(input, "joshua") == 0) {
            const char *status = check_status_from_file("joshua.txt");
            if (strcmp(status, "enabled") == 0) {
                joshua();
                clear_screen();
            } else {
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nIDENTIFICATION NOT RECOGNIZED BY SYSTEM\n--CONNECTION TERMINATED--\n");
                play_sample("./samples/theyve-taken-out-my-password.wav");
                usleep(1000000);
                break;  // Exit the while loop  
            }
        } else if (strcmp(input, "hints") == 0) {
            char buffer[10];
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nSET HINTS (0 = DISABLED, 1 = ENABLED): ");
            fgets(buffer, sizeof(buffer), stdin); 
            // Convert the string to an integer
            hints = atoi(buffer);
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("HINTS UPDATED\n\n"); 
        } else if (strcmp(input, "woprchat") == 0 ||
                   strcmp(input, "webllm") == 0 ||
                   strcmp(input, "gpt") == 0) {
            // "webllm" and "gpt" are retained as hidden backwards-compatible aliases.
            char buffer[10];
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("\nSET WOPR CHAT (0 = DISABLED, 1 = ENABLED): ");
            fgets(buffer, sizeof(buffer), stdin);
            wopr_chat_enabled = atoi(buffer) ? 1 : 0;
            play_sample_async("./samples/computer-beeps-short.wav");
            delayed_print("WOPR CHAT UPDATED\n\n");
        } else {
            if (!authenticateUser(input)) {
                failed_attempts++;
                play_sample_async("./samples/computer-beeps.wav");
                delayed_print("\nIDENTIFICATION NOT RECOGNIZED BY SYSTEM\n");
                if (failed_attempts >= 3) {
                    delayed_print("--CONNECTION TERMINATED--\n");
                    usleep(1000000);
                    break;
                }
                delayed_print("\n");
                press_enter_to_continue();
                clear_screen();
            }
        }
    }

    exit(0);  // Exit the program after breaking out of the while loop
}

int main() {
        fix_backspace_key();
        char command[200];
        // Clear screen
        clear_screen();

        // Send "LOGON: " to the client
        play_sample_async("./samples/computer-beeps.wav");
        int i;
    	for (i = 0; i < 360; i++) {
        delayed_print(" ");
        usleep(500);
    	}
        delayed_print("\n");
        
        // Handle user input
        create_root_user();
        handle_user_input();
        
        exit(0);
      
}
