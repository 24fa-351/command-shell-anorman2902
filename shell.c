#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_INPUT 1024
#define MAX_ARGS 128
#define MAX_ENV_VARS 128

typedef struct {
    char *name;
    char *value;
} EnvVar;

EnvVar env_vars[MAX_ENV_VARS];
int env_var_count = 0;

char *get_env_var(const char *name) {
    for (int i = 0; i < env_var_count; ++i) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

void set_env_var(const char *name, const char *value) {
    for (int i = 0; i < env_var_count; ++i) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return;
        }
    }
    env_vars[env_var_count].name = strdup(name);
    env_vars[env_var_count].value = strdup(value);
    env_var_count++;
}

void unset_env_var(const char *name) {
    for (int i = 0; i < env_var_count; ++i) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].name);
            free(env_vars[i].value);
            for (int j = i; j < env_var_count - 1; ++j) {
                env_vars[j] = env_vars[j + 1];
            }
            env_var_count--;
            return;
        }
    }
}

void replace_vars(char *command) {
    char *pos;
    while ((pos = strstr(command, "$")) != NULL) {
        char var_name[MAX_INPUT] = {0};
        sscanf(pos + 1, "%s", var_name);
        char *value = get_env_var(var_name);
        if (value) {
            char temp[MAX_INPUT];
            snprintf(temp, MAX_INPUT, "%.*s%s%s", (int)(pos - command), command, value, pos + strlen(var_name) + 1);
            strcpy(command, temp);
        } else {
            memmove(pos, pos + strlen(var_name) + 1, strlen(pos + strlen(var_name) + 1) + 1);
        }
    }
}

void execute_command(char *command);

void execute_pipeline(char **commands, int count) {
    int fd[2], in_fd = 0;
    for (int i = 0; i < count; ++i) {
        pipe(fd);
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, 0);
            if (i < count - 1) dup2(fd[1], 1);
            close(fd[0]);
            execute_command(commands[i]);
            exit(1);
        } else {
            wait(NULL);
            close(fd[1]);
            in_fd = fd[0];
        }
    }
}

void execute_command(char *command) {
    char *args[MAX_ARGS];
    int arg_count = 0;
    replace_vars(command);

    char *redirect_in = strstr(command, "<");
    char *redirect_out = strstr(command, ">");
    char *background = strchr(command, '&');

    if (redirect_in) {
        *redirect_in = '\0';
        redirect_in++;
    }

    if (redirect_out) {
        *redirect_out = '\0';
        redirect_out++;
    }

    if (background) {
        *background = '\0';
    }

    char *token = strtok(command, " ");
    while (token) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (strcmp(args[0], "cd") == 0) {
        if (arg_count > 1) {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        } else {
            fprintf(stderr, "cd: missing argument\n");
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[MAX_INPUT];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s\n", cwd);
        }
    } else if (strcmp(args[0], "set") == 0) {
        if (arg_count == 3) {
            set_env_var(args[1], args[2]);
        } else {
            fprintf(stderr, "set: usage: set VAR VALUE\n");
        }
    } else if (strcmp(args[0], "unset") == 0) {
        if (arg_count == 2) {
            unset_env_var(args[1]);
        } else {
            fprintf(stderr, "unset: usage: unset VAR\n");
        }
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            if (redirect_in) {
                int fd_in = open(redirect_in, O_RDONLY);
                if (fd_in < 0) {
                    perror("input redirection");
                    exit(1);
                }
                dup2(fd_in, 0);
                close(fd_in);
            }

            if (redirect_out) {
                int fd_out = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("output redirection");
                    exit(1);
                }
                dup2(fd_out, 1);
                close(fd_out);
            }

            execvp(args[0], args);
            perror("exec");
            exit(1);
        } else {
            if (!background) {
                wait(NULL);
            }
        }
    }
}

int main() {
    char input[MAX_INPUT];

    while (1) {
        printf("xsh# ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }

        char *commands[MAX_ARGS];
        int command_count = 0;

        char *token = strtok(input, "|");
        while (token) {
            commands[command_count++] = token;
            token = strtok(NULL, "|");
        }

        if (command_count > 1) {
            execute_pipeline(commands, command_count);
        } else {
            execute_command(commands[0]);
        }
    }

    return 0;
}
