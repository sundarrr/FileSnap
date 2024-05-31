#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h> // For directory operations
#include <pwd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <string.h>
#include <sys/types.h>



#define PORT 8086
#define MAX_CLIENTS 15
#define CHUNK_SIZE 1024


char* get_home_directory() {
    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL) {
        perror("getpwuid");
        exit(EXIT_FAILURE);
    }
    return pw->pw_dir;
}


int compare_strings(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcasecmp(str1, str2);
}

//dirlist command starts
// Filter function to exclude hidden directories
int filter(const struct dirent *dir) {
    return (dir->d_name[0] != '.');
}

// Comparison function for alphabetical case-insensitive sorting
int case_insensitive_sort(const struct dirent **a, const struct dirent **b) {
    return strcasecmp((*a)->d_name, (*b)->d_name);
}

// Comparison function for sorting by creation time
int sort_by_time(const struct dirent **a, const struct dirent **b) {
    struct stat statA, statB;
    char pathA[1024], pathB[1024];
    
    snprintf(pathA, sizeof(pathA), "%s/%s", get_home_directory(), (*a)->d_name);
    snprintf(pathB, sizeof(pathB), "%s/%s", get_home_directory(), (*b)->d_name);
    
    stat(pathA, &statA);
    stat(pathB, &statB);
    
    return statA.st_ctime > statB.st_ctime;
}

// Modified list_directories function
void list_directories(int client_socket, const char *start_path, const char *sort_option) {
    DIR *dir = opendir(start_path);
    if (dir == NULL) { //If the directory couldn't be opened or doesn't exist
        perror("opendir"); //print error
        return;
    }

    struct dirent **namelist;
    int n;
    // Check the sort_option to decide how to sort directory entries (if it is "a")
    if (strcmp(sort_option, "-a") == 0) {
        // Assume filter and case_insensitive_sort are defined elsewhere
        n = scandir(start_path, &namelist, filter, case_insensitive_sort);
    // Check the sort_option to decide how to sort directory entries (if it is "t")
    } else if (strcmp(sort_option, "-t") == 0) {
        n = scandir(start_path, &namelist, filter, sort_by_time);
    } else {
        n = scandir(start_path, &namelist, filter, alphasort);
    }

    //if scanning the directory fails, 
    if (n < 0) {
        perror("scandir"); //print error
        closedir(dir);
        return;
    }

    char *full_path;
    struct stat info;
    for (int i = 0; i < n; i++) {
        struct dirent *entry = namelist[i];
        if (entry->d_type == DT_DIR) {
            // Allocate memory for the full path of the directory
            full_path = malloc(strlen(start_path) + strlen(entry->d_name) + 2);
            if (full_path == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            sprintf(full_path, "%s/%s", start_path, entry->d_name);
            stat(full_path, &info); // Get file statistics including creation time

            // Check if sorting option is "-t" (by time)
            if (strcmp(sort_option, "-t") == 0) {
                char timebuf[256];
                strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&info.st_ctime));
                send(client_socket, full_path, strlen(full_path), 0);
                send(client_socket, " - Created: ", 12, 0);
                send(client_socket, timebuf, strlen(timebuf), 0);
                send(client_socket, "\n", 1, 0);
            } else {
                send(client_socket, full_path, strlen(full_path), 0);
                send(client_socket, "\n", 1, 0);
            }

             // Recursively list directories with the same sorting option
            list_directories(client_socket, full_path, sort_option); // Recurse with the same sorting option
            free(full_path); //free the memory allocated for full path
        }
        free(namelist[i]);
    }
    free(namelist);
    closedir(dir);
}


// Modify the function to return an int
int search_file_recursive(const char *dir_path, const char *filename, int client_socket) {
     // Open the directory specified by dir_path
    DIR *dir = opendir(dir_path);
    if (dir == NULL) { //if directory couldn't be opened
        perror("opendir"); //print error
        return 0;  // Return 0 indicating file not found here
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Check if the entry is a directory
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char sub_dir_path[1024];
                snprintf(sub_dir_path, sizeof(sub_dir_path), "%s/%s", dir_path, entry->d_name);
                // Recursively search for the file in the subdirectory
                if (search_file_recursive(sub_dir_path, filename, client_socket)) {
                    // If file found in the subdirectory, close the directory and return 1 to stop recursion
                    closedir(dir);
                    return 1;  // Stop recursion when file is found
                }
            }
        } else {
            // Check if the entry is a file and its name matches the desired filename
            if (strcmp(entry->d_name, filename) == 0) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, filename);

                //Get the file details
                struct stat file_stat;
                if (stat(full_path, &file_stat) == -1) {
                    perror("stat");
                    send(client_socket, "Error: File not found\n", strlen("Error: File not found\n"), 0);
                } else {

                    //Gather information about the file
                    char info_buffer[1024];
                    sprintf(info_buffer, "Filename: %s\nSize: %ld bytes\nCreated: %sPermissions: %o\n",
                            filename,
                            file_stat.st_size,
                            ctime(&file_stat.st_ctime),
                            file_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));

                    //Send the gathered information to client
                    send(client_socket, info_buffer, strlen(info_buffer), 0);
                }
                closedir(dir);
                return 1;  // File found, stop further searching
            }
        }
    }
    closedir(dir);
    return 0;  // Continue searching in other directories
}

void send_file_info(const char *filename, int client_socket) {
    // Print a message indicating the file being searched for
    printf("Searching for file: %s\n", filename);
    if (!search_file_recursive(get_home_directory(), filename, client_socket)) {
        //If the file is not found, print appropriate message
        send(client_socket, "File not found\n", strlen("File not found\n"), 0);
    }
}

//Function for creating tar.gz file
void create_tar_gz(const char *dirname, const char *tar_name, long size1, long size2) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "find %s -type f -size +%ldc -size -%ldc -print0 | tar -czvf %s --null -T -", dirname, size1, size2, tar_name);
    printf("Command: %s\n", cmd); // Print the command being executed
    system(cmd);
}

//Function to send the contents of tar file to client using socket
void send_tar_file(int client_socket, const char *tar_name) {
    // Open the tar file
    int tar_fd = open(tar_name, O_RDONLY);
    if (tar_fd == -1) { //If tar file couldn't be opened
        perror("open"); //Print appropriate error message
        return;
    }

    // Get the file size
    struct stat stat_buf;
    fstat(tar_fd, &stat_buf);
    off_t file_size = stat_buf.st_size;

    // Send the file size to the client
    send(client_socket, &file_size, sizeof(off_t), 0);

    // Send the file contents to the client
    off_t offset = 0;
    while (offset < file_size) {
        ssize_t sent_bytes = sendfile(client_socket, tar_fd, &offset, CHUNK_SIZE);
        if (sent_bytes == -1) {
            perror("sendfile");
            close(tar_fd);
            return;
        }
    }

    close(tar_fd);
}

//Function to handle the w24fz command
void handle_w24fz(int client_socket, char *buffer) {
    int size1, size2;
    sscanf(buffer, "w24fz %d %d\n", &size1, &size2);

    // Validate size range
    if (size1 < 0 || size2 < 0 || size1 > size2) { 
        //If size range is invalid, print appropriate message in client 
        char *msg = "Invalid size range provided.\n";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }

    char *homeDir = get_home_directory();
    char w24projectDir[1024];
    snprintf(w24projectDir, sizeof(w24projectDir), "%s/w24project", homeDir);
    mkdir(w24projectDir, 0777); // Ensure the w24project directory exists, if not create it

    // Prepare the tar command
    char tarFilename[1024];
    snprintf(tarFilename, sizeof(tarFilename), "%s/temp.tar.gz", w24projectDir);
    char cmd[2048];

    // Updated find command to exclude directories explicitly
    snprintf(cmd, sizeof(cmd), "find %s -type d -name \".*\" -prune -o -type f -size +%ldc -size -%ldc ! -name \".*\" -print0 | tar -czvf %s --null -T -", homeDir, size1, size2, tarFilename);

    // Execute the command to create the tar file
    if (system(cmd) == -1) {
        perror("Failed to create tar file");
        char *error_msg = "Failed to create tar file.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // Check if the tar file is effectively empty
    struct stat statBuf;
    if (stat(tarFilename, &statBuf) == -1 || statBuf.st_size <= 1) { // Check for minimal size to assume emptiness
        char *msg = "No file found";
        send(client_socket, msg, strlen(msg), 0);
        // Cleanup: Remove the potentially empty tar file
        unlink(tarFilename);
    } else {
        // If the tar file contains data, send it to the client
        send_tar_file(client_socket, tarFilename);
        char *msg = "Tar file received.\n";
        send(client_socket, msg, strlen(msg), 0);
        
    }
}

//Function to handle w24ft command
void handle_w24ft(int client_socket, char *buffer) {
    char findCmd[2048];
    char tarCmd[2048];
    char findCmdPart[1024] = "";
    char *homeDir = get_home_directory();  // Ensure this function correctly fetches the home directory
    char w24projectDir[1024];
    char tarFilename[1024];

    // Ensure the directory exists and prepare the tar file path
    snprintf(w24projectDir, sizeof(w24projectDir), "%s/w24project", homeDir);
    mkdir(w24projectDir, 0777); //If "w24project" doesn't exist, create one
    snprintf(tarFilename, sizeof(tarFilename), "%s/temp.tar.gz", w24projectDir);//file name for tar

// Assume buffer, findCmd, and findCmdPart are adequately sized and initialized
char *token = strtok(buffer + 6, " \n");  // Skip "w24ft " and consider newline
int extCount = 0;
while (token != NULL) {
    if (extCount == 0) {
        snprintf(findCmdPart, sizeof(findCmdPart), "\\( -name '*.%s'", token);
    } else {
        snprintf(findCmdPart + strlen(findCmdPart), sizeof(findCmdPart) - strlen(findCmdPart), " -o -name '*.%s'", token);
    }
    token = strtok(NULL, " \n");  // Proceed to the next extension
    extCount++;
}
if (extCount > 0) {
    strncat(findCmdPart, " \\)", sizeof(findCmdPart) - strlen(findCmdPart) - 1);  // Close the grouping
}

// Construct the find command to check for files existence
// Exclude directories starting with '.' and their contents
snprintf(findCmd, sizeof(findCmd), "find %s -path '*/.*' -prune -o -type f %s -print", homeDir, findCmdPart);

    // Use popen to execute find command and check for output
    FILE *fp = popen(findCmd, "r");
    if (fp == NULL) {
        char *errorMsg = "Failed to execute file search.\n";
        send(client_socket, errorMsg, strlen(errorMsg), 0);
        return;
    }

    // Check if there is any output from the find command (indicating files were found)
    char tempBuf[128];
    if (fgets(tempBuf, sizeof(tempBuf), fp) == NULL) {
        pclose(fp);
        char *msg = "No files found matching the specified extensions.";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }
    pclose(fp);

    // Files found, proceed with tar command
    snprintf(tarCmd, sizeof(tarCmd), "tar -czvf %s --files-from /dev/null", tarFilename); // Initialize tar with no files
    system(tarCmd); // Create an empty tar file first to avoid "No files found" error from tar
    snprintf(tarCmd, sizeof(tarCmd), "find %s -type f %s -print0 | tar -czvf %s --null -T -", homeDir, findCmdPart, tarFilename);

    // Execute the tar command
    if (system(tarCmd) != 0) {
        char *errorMsg = "Failed to create tar file.\n";
        send(client_socket, errorMsg, strlen(errorMsg), 0);
        return;
    }

    // Send the tar file
    send_tar_file(client_socket, tarFilename);
}

//Function for handling w24fdb command
void handle_w24fdb(int client_socket, char *buffer) {
    char *date = buffer + 7; // Skip past "w24fdb " to start of date
    date[strcspn(date, "\n")] = 0; // Remove newline character at the end

    char findCmd[2048];
    char tarFilename[1024];
    char *homeDir = get_home_directory(); // Implement this function as needed
    char w24projectDir[1024];

    // Ensure the directory exists and prepare the tar file path
    snprintf(w24projectDir, sizeof(w24projectDir), "%s/w24project", homeDir);
    mkdir(w24projectDir, 0777);
    snprintf(tarFilename, sizeof(tarFilename), "%s/temp.tar.gz", w24projectDir);

    // Construct the find command to list files created on or before the provided date
    snprintf(findCmd, sizeof(findCmd), "find %s -type f ! -newermt '%s' -print0", homeDir, date);

    // Use popen to execute find command and check for output
    FILE *fp = popen(findCmd, "r");
    if (fp == NULL) {
        char *errorMsg = "Failed to search for files.\n";
        send(client_socket, errorMsg, strlen(errorMsg), 0);
        return;
    }

    char tempBuf[128];
    if (fgets(tempBuf, sizeof(tempBuf), fp) == NULL) {
        pclose(fp);
        char *msg = "No files found created on or before the specified date.";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }
    pclose(fp);

    // Files found, proceed with creating the tar file
    snprintf(findCmd, sizeof(findCmd), "find %s -type f ! -newermt '%s' -print0 | tar -czvf %s --null -T -", homeDir, date, tarFilename);
    // Execute the command to create the tar file
    if (system(findCmd) == -1) {
        perror("Failed to create tar file");
        char *error_msg = "Failed to create tar file.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // Check if the tar file is effectively empty
    struct stat statBuf;
    if (stat(tarFilename, &statBuf) == -1 || statBuf.st_size <= 1) { // Check for minimal size to assume emptiness
        char *msg = "No file found";
        send(client_socket, msg, strlen(msg), 0);
        // Cleanup: Remove the potentially empty tar file
        unlink(tarFilename);
    } else {
        // If the tar file contains data, send it to the client
        send_tar_file(client_socket, tarFilename);
        char *msg = "Tar file received.\n";
        send(client_socket, msg, strlen(msg), 0);
        
    }
}

//Function for w24fda command
void handle_w24fda(int client_socket, char *buffer) {
    char *date = buffer + 7; // Skip past "w24fda " to start of date
    date[strcspn(date, "\n")] = 0; // Remove newline character at the end

    char findCmd[2048];
    char tarFilename[1024];
    char *homeDir = get_home_directory(); // Implement this function as needed
    char w24projectDir[1024];

    snprintf(w24projectDir, sizeof(w24projectDir), "%s/w24project", homeDir);
    mkdir(w24projectDir, 0777);
    snprintf(tarFilename, sizeof(tarFilename), "%s/temp.tar.gz", w24projectDir);

snprintf(findCmd, sizeof(findCmd), "find %s -path '*/.*' -prune -o -type f ! -name '.*' -newermt '%s' -print0", homeDir, date);

    FILE *fp = popen(findCmd, "r");
    if (fp == NULL) {
        char *errorMsg = "Failed to search for files.\n";
        send(client_socket, errorMsg, strlen(errorMsg), 0);
        return;
    }

    char tempBuf[128];
    if (fgets(tempBuf, sizeof(tempBuf), fp) == NULL) {
        pclose(fp);
        char *msg = "No files found created on or after the specified date.";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }
    pclose(fp);

    snprintf(findCmd, sizeof(findCmd), "find %s -type f -newermt '%s' -print0 | tar -czvf %s --null -T -", homeDir, date, tarFilename);
    // Execute the command to create the tar file
    if (system(findCmd) == -1) {
        perror("Failed to create tar file");
        char *error_msg = "Failed to create tar file.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // Check if the tar file is effectively empty
    struct stat statBuf;
    if (stat(tarFilename, &statBuf) == -1 || statBuf.st_size <= 1) { // Check for minimal size to assume emptiness
        char *msg = "No file found";
        send(client_socket, msg, strlen(msg), 0);
        // Cleanup: Remove the potentially empty tar file
        unlink(tarFilename);
    } else {
        // If the tar file contains data, send it to the client
        send_tar_file(client_socket, tarFilename);
        char *msg = "Tar file received.\n";
        send(client_socket, msg, strlen(msg), 0);
        
    }
}
//end of w24da

void crequest(int client_socket) {
    char buffer[1024] = {0};
    int valread;
    const char *END_MARKER = "\nEND_OF_RESPONSE\n";
    while(1) {
        printf("serverw24$ Waiting for command from client...\n");
memset(buffer, 0, sizeof(buffer));  
        // Reading data from the client
        valread = read(client_socket, buffer, 1024);
        if (valread <= 0) {
            // Client disconnected or error occurred
            printf("Client disconnected or error occurred. Exiting crequest()...\n");
            break;
        }
        
        
         buffer[valread] = '\0';  // Ensure string is null-terminated
    printf("Raw server buffer: [%s]\n", buffer);
     buffer[valread] = '\0';  // Ensure string is null-terminated
    printf("Raw server buffer: [%s]\n", buffer);
    printf("serverw24$ Processed message from client: '%s'\n", buffer);
        
        // Check if the received message is "quitc"
        if (strcmp(buffer, "quitc") == 0) {
            printf("Client requested to quit. Exiting crequest()...\n");
            break;
        }
        // If command is dirlist -a
        else if (strncmp(buffer, "dirlist -a",10) == 0) {
            printf("Executing dirlist -a command...\n");
            list_directories(client_socket,get_home_directory(),"-a");
	    send(client_socket, END_MARKER, strlen(END_MARKER), 0); // Send the end marker
            printf("Directory list sent to client.\n");
        }

        // If command is dirlist -t
        else if (strncmp(buffer, "dirlist -t",10) == 0) {
            printf("Executing dirlist -a command...\n");
            list_directories(client_socket,get_home_directory(),"-t");
            send(client_socket, END_MARKER, strlen(END_MARKER), 0); // Send the end marker
            printf("Directory list sent to client.\n");
        }

        // If command is w24fn
        else if (strncmp(buffer, "w24fn ", 6) == 0) {
            char *filename = buffer + 6; // Extract filename from command
            filename[strlen(filename) - 1] = '\0'; // Remove the newline character
            printf("Searching for file: %s\n", filename);
            send_file_info(filename, client_socket);
            send(client_socket, END_MARKER, strlen(END_MARKER), 0); // Send the end marker
            printf("File info sent to client.\n");
        }

        // If command is w24fz
        else if (strncmp(buffer, "w24fz ", 6) == 0) {
            handle_w24fz(client_socket, buffer);
        }

        // If command is w24ft
        else if (strncmp(buffer, "w24ft ", 6) == 0) {
        handle_w24ft(client_socket, buffer);
        
    }

    // If command is w24fdb
    else if (strncmp(buffer, "w24fdb ", 6) == 0) {
        handle_w24fdb(client_socket, buffer);
       
    }

    // If command is w24fda
   else if (strncmp(buffer, "w24fda ", 7) == 0) {
        handle_w24fda(client_socket, buffer);
    
    }

    }

    // Close the client socket after exiting the loop
    close(client_socket);
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
      char buffer[1024] = {0};
    int opt = 1;
   
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to localhost port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listening for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while(1) {
        printf("Waiting for client request...\n");

        // Accepting incoming connections
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New client connected\n");

            // Handle the connection directly
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {  // Child process
                close(server_fd);  // Close the server socket in the child process
                crequest(new_socket);  // Handle the request
                exit(EXIT_SUCCESS);
            } else {  // Parent process
                close(new_socket);  // Close the client socket in the parent process
            }
        }
 
    return 0;
}

