#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>


#define PORT 8084
#define CHUNK_SIZE 1024

void print_help() {
    printf("COMMANDS\n");
    printf("dirlist -a\n");
    printf("   Description: Lists directories and subdirectories in the home directory in alphabetical order.\n\n");
    printf("dirlist -t\n");
    printf("   Description: Lists directories and subdirectories based on the time they were created, oldest first.\n\n");
    printf("w24fn <filename>\n");
    printf("   Description: Searches for a file and returns its details if found.\n\n");
    printf("w24fz <size1> <size2>\n");
    printf("   Description: Finds files within a size range and packages them into a .tar.gz archive.\n\n");
    printf("w24ft <extension list>\n");
    printf("   Description: Packages files of certain types into a temp.tar.gz archive.\n\n");
    printf("w24fdb <date>\n");
    printf("   Description: Returns files created on or before a specified date in a temp.tar.gz archive.\n\n");
    printf("w24fda <date>\n");
    printf("   Description: Returns files created on or after a specified date in a temp.tar.gz archive.\n\n");
    printf("quitc\n");
    printf("   Description: Terminates the client process.\n\n");
}

//Function to count the number of tokens in a given string
int countTokens(const char *input) {
    int count = 0;
    const char *temp = input;
    while (*temp) { // Loop till end of string is reached
        // Skip leading spaces
        while (*temp == ' ' && *temp != '\0') {
            temp++;
        }
        // If current character isn't end of string, we're at the start of a word
        if (*temp != '\0') {
            count++;
            // Skip current word
            while (*temp != ' ' && *temp != '\0') {
                temp++;
            }
        }
    }
    return count;
}

// Function to validate a command with exactly one argument
int validateCommandWithOneArg(const char *input) {
    if (countTokens(input) == 2) { //To check if number of tokens is exactly 2
        return 1;  // Validation successful
    } else {
        printf("Error: Command requires exactly one argument.\n");
        return 0;  // Validation failed
    }
}

//Function to validate w24ft command
int validateW24ft(const char *input) {
    //Check if the number of tokens (words) in the input string is between 2 and 4
    if (countTokens(input)>= 2 && countTokens(input) <=4) {
        return 1;  // Validation successful
    } else {
        printf("Error: Command can should have 1 -3  arguments\n");
        return 0;  // Validation failed
    }
}

//Function to validate w24fz command
int validateW24fz(const char *input) {
    //Check if the number of tokens is exactly 3
    if (countTokens(input) == 3) {
        return 1;  // Validation successful
    } else {
        printf("Error: Command requires exactly two arguments.\n");
        return 0;  // Validation failed
    }
}

// Function to validate a date string in the format YYYY-MM-DD
int isValidDate(const char *date) {
    // Trim leading whitespace
    while (isspace((unsigned char)*date)) date++;

    // Check basic format and length
    if (strlen(date) != 10) {
        printf("Error: Date length is incorrect. Found length: %lu, expected: 10.\n", strlen(date));
        return 0;
    }

    // Check format YYYY-MM-DD
    for (int i = 0; i < 10; i++) {
        if ((i == 4 || i == 7) && date[i] != '-') {
            printf("Error: Date format is incorrect. Expected '-' at positions 5 and 8.\n");
            return 0; // Ensure '-' in the correct positions
        } else if ((i != 4 && i != 7) && !isdigit(date[i])) {
            printf("Error: Date format is incorrect. Non-digit characters found.\n");
            return 0; // Ensure digits in all other positions
        }
    }

    // After format checks, ensure no additional characters after date
    if (date[11] != '\0') {
        printf("Error: Extra characters after valid date.\n");
        return 0;
    }

    return 1; // Date is valid
}

// Function to validate a command with a date argument
int validateDateCommand(const char *input) {
    // Find the first space to separate command from date
    const char *space = strchr(input, ' ');
    if (space == NULL) {
        printf("Error: No space found separating command and date.\n");
        return 0;
    }

    // Move past the space to the start of the date
    const char *date = space + 1;

    // Trim any leading spaces before the date
    while (isspace((unsigned char)*date)) date++;

    // Ensure there's only one space between command and date
    if (date == space + 1) {
        //Validate the format of date
        if (isValidDate(date)) {
            printf("Valid command and date format.\n");
            return 1;
        }
    }

    printf("Error: Invalid command or date format.\n");
    return 0;
}

// Function to receive a file from a socket and save it to disk
void receive_file(int sock, const char *filename) {

    // Open the file for writing, creating it if it doesn't exist, and truncating it to zero length
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        return;
    }

    //Get the file size from socket
    off_t file_size;
    recv(sock, &file_size, sizeof(off_t), 0);

    // Initialize remaining bytes to be received
    off_t remaining = file_size;
    while (remaining > 0) {
        char buffer[CHUNK_SIZE];
        //Recieve data from socket
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Connection closed by server.\n");
            } else {
                perror("recv");
            }
            close(fd);
            return;
        }

        //Write the recieved data into file
        ssize_t bytes_written = write(fd, buffer, bytes_received);
        if (bytes_written == -1) {
            perror("write");
            close(fd);
            return;
        }

        // Update remaining bytes to be received
        remaining -= bytes_written;
    }

    printf("File received: %s\n", filename);

    close(fd);
}

void connectAndHandle(int port) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char message[1024];

    // Creating socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        return;
    }

    // Connecting to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        return;
    }

    // Special handling for initial coordinator connection
    if (port == PORT) {
        // Read the port number for the next server
        valread = read(sock, buffer, 1024);
        int nextPort = atoi(buffer);
  // Close the initial connection
        if (nextPort != PORT) {
            // If the next port is different, connect to the new server
            close(sock); 

            connectAndHandle(nextPort);
            return;
        }
    }

  int command_sent = 0;
    // Begin user input loop for command execution
memset(buffer, 0, sizeof(buffer));
    while(1) {
        printf("clientw24$ ");
        fgets(message, sizeof(message), stdin); // Getting user input

     
        // Check for quit command
        if (strcmp(message, "quitc\n") == 0) {
            printf("Exiting connection...\n");
            break;
        }

        else if (strcmp(message, "dirlist -a\n") == 0) {
            printf("Requesting directory list from server...\n");
            // Sending the command to the server
            send(sock, message, strlen(message), 0);

            // Read directory list from the server
            printf("Directory list received from server:\n");
             int end_received = 0;
    while (!end_received) {
        valread = read(sock, buffer, 1023); // Read data into buffer, leaving space for null terminator
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the received data
            // Check if the end marker is in the buffer
            if (strstr(buffer, "\nEND_OF_RESPONSE\n") != NULL) {
                *strstr(buffer, "\nEND_OF_RESPONSE\n") = '\0'; // Cut off the marker at its start
                end_received = 1; // Set flag to exit the loop
            }
            printf("%s", buffer); // Print the received data
        } else if (valread <= 0) {
            perror("read");
            break; // Break from the loop in case of read error or disconnection
        }
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer for the next read
    }      
        }
        else if (strcmp(message, "dirlist -t\n") == 0) {
            printf("Requesting directory list from server...\n");
            // Sending the command to the server
            send(sock, message, strlen(message), 0);

            // Read directory list from the server
            printf("Directory list received from server:\n");
            int end_received = 0;
    while (!end_received) {
        valread = read(sock, buffer, 1023); // Read data into buffer, leaving space for null terminator
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the received data
            // Check if the end marker is in the buffer
            if (strstr(buffer, "\nEND_OF_RESPONSE\n") != NULL) {
                *strstr(buffer, "\nEND_OF_RESPONSE\n") = '\0'; // Cut off the marker at its start
                end_received = 1; // Set flag to exit the loop
            }
            printf("%s", buffer); // Print the received data
        } else if (valread <= 0) {
            perror("read");
            break; // Break from the loop in case of read error or disconnection
        }
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer for the next read
    }
        }
        else if (strncmp(message, "w24fn ", 6) == 0) {
        
        if (!validateCommandWithOneArg(message)) {
        
    }
        else {
            printf("Requesting file information from server...\n");
            // Sending the command to the server
            send(sock, message, strlen(message), 0);
            command_sent = 1; // Set flag to indicate command has been sent

            // Read file information from the server
            printf("File information received from server:\n");
            int end_received = 0;
    while (!end_received) {
        valread = read(sock, buffer, 1023); // Read data into buffer, leaving space for null terminator
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the received data
            // Check if the end marker is in the buffer
            if (strstr(buffer, "\nEND_OF_RESPONSE\n") != NULL) {
                *strstr(buffer, "\nEND_OF_RESPONSE\n") = '\0'; // Cut off the marker at its start
                end_received = 1; // Set flag to exit the loop
            }
            printf("%s", buffer); // Print the received data
        } else if (valread <= 0) {
            perror("read");
            break; // Break from the loop in case of read error or disconnection
        }
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer for the next read
    }
    
   }
    
            }
        else if (strncmp(message, "w24fz ", 6) == 0) {
         if (!validateW24fz(message)) {
    }
    else
    {
    printf("Requesting files within size range from server...\n");
    send(sock, message, strlen(message), 0); // Send the w24fz command
    receive_file(sock, "temp.tar.gz"); // Expect to receive tar.gz or a "no file found" indication
    }
}

else if (strncmp(message, "w24ft ", 6) == 0) {

if (!validateW24ft(message)) {
       
    }
        else 
{
    printf("Requesting files of specified types from server...\n");
    send(sock, message, strlen(message), 0); // Send the w24ft command

    // Directly read the response
    char response[CHUNK_SIZE] = {0};
    int bytes_read = recv(sock, response, sizeof(response), 0);

    if (bytes_read <= 0) {
        // Error handling
        perror("Error receiving response from server");
        continue;
    }

    // Attempt to interpret the response as a file size (assuming error messages won't be 8 bytes exactly)
    off_t file_size;
    memcpy(&file_size, response, sizeof(file_size));
    
    if (file_size > 0 && file_size != (off_t)-1) { // Check for a valid, non-error file size
        // Assume it's a file size and proceed to receive the file
        receive_file(sock, "temp.tar.gz");
    } else {
        // Assume it's an error message and print it
        printf("%s\n", response);
    }
    }
}
else if (strncmp(message, "w24fdb ", 7) == 0) {

if (!validateCommandWithOneArg(message)) {
       
    }
        else 
        {
    printf("Requesting files of specified types from server...\n");
    send(sock, message, strlen(message), 0); 

    // Directly read the response
    char response[CHUNK_SIZE] = {0};
    int bytes_read = recv(sock, response, sizeof(response), 0);

    if (bytes_read <= 0) {

        perror("Error receiving response from server");
        continue;
    }

    // Attempt to interpret the response as a file size (assuming error messages won't be 8 bytes exactly)
    off_t file_size;
    memcpy(&file_size, response, sizeof(file_size));
    
    if (file_size > 0 && file_size != (off_t)-1) { // Check for a valid, non-error file size
        // Assume it's a file size and proceed to receive the file
        receive_file(sock, "temp.tar.gz");
    } else {
        // Assume it's an error message and print it
        printf("%s\n", response);
    }
    }
}
else if (strncmp(message, "w24fda ", 7) == 0) {
if (!validateCommandWithOneArg(message)) {
        
    }
        else {
    printf("Requesting files of specified types from server...\n");
    send(sock, message, strlen(message), 0); // Send the w24ft command

    // Directly read the response
    char response[CHUNK_SIZE] = {0};
    int bytes_read = recv(sock, response, sizeof(response), 0);

    if (bytes_read <= 0) {
        // Error handling
        perror("Error receiving response from server");
        continue;
    }

    // Attempt to interpret the response as a file size (assuming error messages won't be 8 bytes exactly)
    off_t file_size;
    memcpy(&file_size, response, sizeof(file_size));
    
    if (file_size > 0 && file_size != (off_t)-1) { // Check for a valid, non-error file size
        // Assume it's a file size and proceed to receive the file
        receive_file(sock, "temp.tar.gz");
    } else {
        // Assume it's an error message and print it
        printf("%s\n", response);
    }
    
    }
}
else
{
printf("You have entered an invalid command . Please refer below:");
print_help();
}

        printf("\n");

        memset(buffer, 0, sizeof(buffer));
    }

    // Close the connection
    close(sock);
}

int main() {
    // Initially connect to the coordinator server
    connectAndHandle(PORT);
    return 0;
}


