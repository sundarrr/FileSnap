---

# FileSnap

## Overview
This project features a robust client-server architecture allowing clients to request and receive files from a server. The server, supported by two mirror servers, efficiently handles multiple client requests using socket communication.

## Table of Contents
- [Features](#features)
- [Technologies Used](#technologies-used)
- [Setup](#setup)
- [Usage](#usage)
  - [Client Commands](#client-commands)
- [Alternating Server Handling](#alternating-server-handling)
- [Submission Requirements](#submission-requirements)
- [License](#license)

## Features
- 🚀 **Multi-client support:** Seamless handling of multiple client requests.
- 🎯 **Command-based file retrieval:** Retrieve files by name, size, type, and date.
- 🌐 **Mirroring:** Load distribution through server mirroring.

## Technologies Used
- **C Programming Language**
- **Socket Programming**

## Setup
### Prerequisites
- GCC Compiler
- Linux environment

### Steps
1. **Clone the repository:**
   ```sh
   git clone https://github.com/username/repository.git
   cd repository
   ```

2. **Compile the server and client code:**
   ```sh
   gcc serverw24.c -o serverw24
   gcc clientw24.c -o clientw24
   gcc mirror1.c -o mirror1
   gcc mirror2.c -o mirror2
   ```

3. **Run the servers on different terminals/machines:**
   ```sh
   ./serverw24
   ./mirror1
   ./mirror2
   ```

4. **Run the client:**
   ```sh
   ./clientw24
   ```

## Usage
### Client Commands
1. **List directories alphabetically:**
   ```sh
   dirlist -a
   ```
   Lists subdirectories in the home directory in alphabetical order.

2. **List directories by creation time:**
   ```sh
   dirlist -t
   ```
   Lists subdirectories in the home directory by creation time, oldest first.

3. **Fetch file details by name:**
   ```sh
   w24fn filename
   ```
   Retrieves details of the specified file if it exists in the server's directory tree.

4. **Fetch files by size range:**
   ```sh
   w24fz size1 size2
   ```
   Retrieves files within the specified size range and compresses them into `temp.tar.gz`.

5. **Fetch files by type:**
   ```sh
   w24ft extension1 extension2 extension3
   ```
   Retrieves files of specified types and compresses them into `temp.tar.gz`.

6. **Fetch files created before a date:**
   ```sh
   w24fdb YYYY-MM-DD
   ```
   Retrieves files created before the specified date and compresses them into `temp.tar.gz`.

7. **Fetch files created after a date:**
   ```sh
   w24fda YYYY-MM-DD
   ```
   Retrieves files created after the specified date and compresses them into `temp.tar.gz`.

8. **Quit the client:**
   ```sh
   quitc
   ```
   Terminates the client process.

### Note
All files returned from the server will be stored in a folder named `w24project` in the client's home directory.

## Alternating Server Handling
- The first three client connections are handled by `serverw24`.
- The next three connections are handled by `mirror1`.
- The following three connections are handled by `mirror2`.
- Subsequent connections are handled in a round-robin manner between `serverw24`, `mirror1`, and `mirror2`.

---
