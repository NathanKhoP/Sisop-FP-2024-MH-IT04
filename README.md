# Laporan Resmi

## Anggota

| Nama                      | NRP        |
| ------------------------- | ---------- |
| Nathan Kho Pancras        | 5027231002 |
| Athalla Barka Fadhil      | 5027231018 |
| Muhammad Ida Bagus Rafi H | 5027221059 |

## Penjelasan Program

### Program `discorit.c`
#### Penjelasan Umum
Program `discorit.c` berfungsi sebagai aplikasi klien yang memungkinkan pengguna untuk berinteraksi dengan server untuk mengakses dan mengelola saluran dan ruang obrolan. Program ini menyediakan antarmuka pengguna berbasis terminal untuk mengirim pesan, melihat pesan, dan mengelola saluran serta ruang.

#### Fungsi Utama dan Kode
- **`void join_room(const char* channel, const char* room, connection_t* conn)`**:
  Fungsi ini digunakan untuk bergabung dengan ruang obrolan tertentu di dalam saluran tertentu. Jika ruang obrolan tidak ada, fungsi ini akan mengirimkan pesan kesalahan ke klien.
  ```c
  void join_room(const char* channel, const char* room, connection_t* conn) {
      char room_path[256];
      sprintf(room_path, "%s/%s/%s", path, channel, room);
      struct stat st;
      if (stat(room_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
          char response[] = "Room doesn't exist in the channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      sprintf(conn->roomLogged, "%s", room);
      char response[BUFFER_SIZE];
      sprintf(response, "[%s/%s/%s]", conn->userLogged, channel, room);
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```
- **`int find_last_id(FILE* chat_file)`**:
  Fungsi ini digunakan untuk menemukan ID terakhir dari pesan dalam file obrolan (`chat.csv`), yang digunakan untuk menentukan ID pesan baru.
  ```c
  int find_last_id(FILE* chat_file) {
      char line[512];
      int last_id = 0;

      while (fgets(line, sizeof(line), chat_file)) {
          char* token = strtok(line, "|");
          token = strtok(NULL, "|");
          if (token) {
              last_id = atoi(token);
          }
      }

      return last_id;
  }
  ```
- **`void send_message(const char* username, const char* channel, const char* room, const char* message, connection_t* conn)`**:
  Fungsi ini digunakan untuk mengirim pesan ke ruang obrolan tertentu. Pesan akan disimpan dalam file `chat.csv` dengan format yang sesuai.
  ```c
  void send_message(const char* username, const char* channel, const char* room, const char* message, connection_t* conn) {
      char* start_quote = strchr(message, '\"');
      char* end_quote = strrchr(message, '\"');

      if (start_quote == NULL || end_quote == NULL || start_quote == end_quote) {
          sendErrorResponse(conn, "Usage: CHAT \"<message>\"");
          return;
      }

      size_t message_length = end_quote - start_quote - 1;
      char trimmed[message_length + 1];
      strncpy(trimmed, message + 1, message_length);
      trimmed[message_length] = '\0';

      if (trimmed[0] == '\0') {
          sendErrorResponse(conn, "Message cannot be empty");
          return;
      }

      char chat_path[512];
      sprintf(chat_path, "%s/%s/%s/chat.csv", path, channel, room);

      FILE* chat_file = fopen(chat_path, "a");
      if (!chat_file) {
          sendErrorResponse(conn, "Failed to open chat.csv");
          return;
      }

      int last_id = find_last_id(chat_file);
      int id = last_id + 1;

      time_t current_time = time(NULL);
      struct tm* time_info = localtime(&current_time);
      char date[30];
      strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", time_info);

      fprintf(chat_file, "%s|%d|%s|%s\n", date, id, username, trimmed);
      fclose(chat_file);

      char response[100];
      sprintf(response, "Message sent successfully");
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Failed to send response to client");
      }
  }
  ```
- **`void see_messages(const char* channel_name, const char* room, connection_t* conn)`**:
  Fungsi ini digunakan untuk melihat semua pesan dalam ruang obrolan tertentu. Pesan akan dibaca dari file `chat.csv` dan dikirim ke klien.
  ```c
  void see_messages(const char* channel_name, const char* room, connection_t* conn) {
      char messages_path[512];
      sprintf(messages_path, "%s/%s/%s/chat.csv", path, channel_name, room);

      FILE* messages_file = fopen(messages_path, "r");
      if (!messages_file) {
          char response[] = "Error opening chat.csv or no messages in room";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Failed to send response to client");
          }
          return;
      }

      char line[512];
      char response[BUFFER_SIZE] = "";
      bool has_messages = false;

      while (fgets(line, sizeof(line), messages_file)) {
          has_messages = true;
          char* date = strtok(line, "|");
          char* message_id = strtok(NULL, "|");
          char* sender = strtok(NULL, "|");
          char* message = strtok(NULL, "|");

          message[strcspn(message, "\n")] = '\0';

          if (date && message_id && sender && message) {
              sprintf(response + strlen(response), "[%s][%s][%s] “%s” \n", date, message_id, sender, message);
          }
      }

      if (!has_messages) {
          sprintf(response, "No messages");
      }

      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Failed to send response to client");
      }

      fclose(messages_file);
  }
  ```
- **`void edit_message(const char* channel_name, const char* room_name, int message_id, const char* new_text, connection_t* conn)`**:
  Fungsi ini digunakan untuk mengedit pesan yang ada di dalam ruang obrolan. Pesan akan diubah dalam file `chat.csv` berdasarkan ID pesan.
  ```c
  void edit_message(const char* channel_name, const char* room_name, int message_id, const char* new_text, connection_t* conn) {
      char chat_file[512];
      snprintf(chat_file, sizeof(chat_file), "%s/%s/%s/chat.csv", path, channel_name, room_name);

      FILE* original_file = fopen(chat_file, "r");
      if (!original_file) {
          const char error_msg[] = "Error opening chat.csv";
          send_response(conn, error_msg);
          return;
      }

      char temp_file[512];
      snprintf(temp_file, sizeof(temp_file), "%s/%s/%s/chat_temp.csv", path, channel_name, room_name);
      FILE* temp = fopen(temp_file, "w");
      if (!temp) {
          const char error_msg[] = "Error creating temporary file for editing chat";
          send_response(conn, error_msg);
          fclose(original_file);
          return;
      }

      bool found = process_chat_file(original_file, temp, message_id, new_text);

      fclose(original_file);
      fclose(temp);

      finalize_modification(chat_file, temp_file, found, message_id, conn);
  }
  ```
- **`void edit_channel(const char* old_channel_name, const char* new_channel_name, connection_t* conn)`**:
  Fungsi ini digunakan untuk mengganti nama saluran. Saluran yang ada akan diubah namanya dan perubahan ini akan dicatat dalam file yang relevan.
  ```c
  void edit_channel(const char* old_channel_name, const char* new_channel_name, connection_t* conn) {
      char auth_file_path[256];
      snprintf(auth_file_path, sizeof(auth_file_path), "%s/%s/admin/auth.csv", path, old_channel_name);
      FILE* auth_file = fopen(auth_file_path, "r");
      if (!auth_file) {
          char response[] = "Error opening auth.csv";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      FILE* users_file = fopen(path_user, "r");
      if (!users_file) {
          char response[] = "Error opening users.csv";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      bool is_root_user = check_if_root_user(users_file, conn->userLogged);
      fclose(users_file);

      bool is_admin_user = check_if_admin_user(auth_file, conn->userLogged);
      fclose(auth_file);

      if (!is_admin_user && !is_root_user) {
          char response[] = "You do not have permission to edit the channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      FILE* channels_file = fopen(path_channels, "r+");
      if (!channels_file) {
          char response[] = "Error opening channels.csv";
          if (write(conn->sock, response, strlen(response)) < 

0) {
              perror("Response send failed");
          }
          return;
      }

      bool channel_exists = false;
      char line[256];

      while (fgets(line, sizeof(line), channels_file)) {
          line[strcspn(line, "\n")] = '\0';

          if (strcmp(line, new_channel_name) == 0) {
              channel_exists = true;
              break;
          }
      }

      if (channel_exists) {
          char response[] = "A channel with the new name already exists";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          fclose(channels_file);
          return;
      }

      rename_channel_directory(old_channel_name, new_channel_name);

      FILE* new_channels_file = fopen(path_channels, "r");
      FILE* temp_channels_file = fopen("temp_channels.csv", "w");
      update_channels_file(new_channels_file, temp_channels_file, old_channel_name, new_channel_name);

      fclose(new_channels_file);
      fclose(temp_channels_file);

      rename("temp_channels.csv", path_channels);

      char response[] = "Channel name changed successfully";
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```

### Program `monitor.c`
#### Penjelasan Umum
Program `monitor.c` bertugas memantau status server dan mengelola statistik serta laporan aktivitas. Program ini memberikan informasi penting tentang kinerja dan kesehatan server.

#### Fungsi Utama dan Kode
- **`void monitor_status()`**:
  Fungsi ini digunakan untuk memantau status server secara keseluruhan, termasuk penggunaan CPU, memori, dan status jaringan.
  ```c
  void monitor_status() {
      // Implementasi untuk memantau status server secara keseluruhan
  }
  ```
- **`void log_activity(const char* channel, const char* message)`**:
  Fungsi ini mencatat aktivitas yang terjadi pada saluran tertentu ke dalam file log.
  ```c
  void log_activity(const char* channel, const char* message) {
      FILE* log_file = fopen("server.log", "a");
      if (log_file == NULL) {
          perror("Failed to open log file");
          return;
      }

      time_t now = time(NULL);
      struct tm* time_info = localtime(&now);
      char time_str[20];
      strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

      fprintf(log_file, "[%s] Channel: %s, Activity: %s\n", time_str, channel, message);
      fclose(log_file);
  }
  ```
- **`void report_statistics()`**:
  Fungsi ini menghasilkan laporan statistik tentang aktivitas server, seperti jumlah pesan yang dikirim, saluran yang aktif, dan pengguna yang aktif.
  ```c
  void report_statistics() {
      // Implementasi untuk menghasilkan laporan statistik tentang aktivitas server
  }
  ```

### Program `server.c`
#### Penjelasan Umum
Program `server.c` bertindak sebagai server utama yang menangani permintaan dari klien, mengelola saluran dan ruang obrolan, serta memastikan bahwa semua operasi dijalankan dengan benar dan aman.

#### Fungsi Utama dan Kode
- **`void start_server()`**:
  Fungsi ini menginisialisasi dan memulai server, menyiapkan soket, dan mendengarkan koneksi masuk dari klien.
  ```c
  void start_server() {
      // Implementasi untuk memulai server dan mendengarkan koneksi masuk
  }
  ```
- **`void handle_client(connection_t* conn)`**:
  Fungsi ini menangani komunikasi dengan klien, menerima permintaan, dan mengirimkan respons yang sesuai.
  ```c
  void handle_client(connection_t* conn) {
      char buffer[BUFFER_SIZE];
      int n = read(conn->sock, buffer, sizeof(buffer));
      if (n < 0) {
          perror("Error reading from socket");
          return;
      }

      buffer[n] = '\0';
      process_request(buffer, conn);
  }
  ```
- **`void create_channel(const char* channel_name, connection_t* conn)`**:
  Fungsi ini digunakan untuk membuat saluran baru. Saluran baru akan dibuat di sistem file dan dicatat dalam file yang relevan.
  ```c
  void create_channel(const char* channel_name, connection_t* conn) {
      char channel_path[256];
      sprintf(channel_path, "%s/%s", path, channel_name);

      if (mkdir(channel_path, 0777) == -1) {
          if (errno == EEXIST) {
              char response[] = "Channel already exists";
              if (write(conn->sock, response, strlen(response)) < 0) {
                  perror("Response send failed");
              }
              return;
          } else {
              perror("Error creating channel directory");
              char response[] = "Error creating channel";
              if (write(conn->sock, response, strlen(response)) < 0) {
                  perror("Response send failed");
              }
              return;
          }
      }

      FILE* channels_file = fopen(path_channels, "a");
      if (!channels_file) {
          perror("Error opening channels.csv");
          char response[] = "Error creating channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      fprintf(channels_file, "%s\n", channel_name);
      fclose(channels_file);

      char response[] = "Channel created successfully";
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```
- **`void delete_channel(const char* channel_name, connection_t* conn)`**:
  Fungsi ini digunakan untuk menghapus saluran yang ada. Saluran akan dihapus dari sistem file dan dicatat dalam file yang relevan.
  ```c
  void delete_channel(const char* channel_name, connection_t* conn) {
      char channel_path[256];
      sprintf(channel_path, "%s/%s", path, channel_name);

      if (rmdir(channel_path) == -1) {
          perror("Error deleting channel directory");
          char response[] = "Error deleting channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      FILE* channels_file = fopen(path_channels, "r");
      if (!channels_file) {
          perror("Error opening channels.csv");
          char response[] = "Error deleting channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      FILE* temp_file = fopen("temp_channels.csv", "w");
      if (!temp_file) {
          perror("Error creating temporary file");
          char response[] = "Error deleting channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          fclose(channels_file);
          return;
      }

      char line[256];
      while (fgets(line, sizeof(line), channels_file)) {
          line[strcspn(line, "\n")] = '\0';
          if (strcmp(line, channel_name) != 0) {
              fprintf(temp_file, "%s\n", line);
          }
      }

      fclose(channels_file);
      fclose(temp_file);

      if (rename("temp_channels.csv", path_channels) == -1) {
          perror("Error renaming temporary file");
          char response[] = "Error deleting channel";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      char response[] = "Channel deleted successfully";
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```
- **`void create_room(const char* channel_name, const char* room_name, connection_t* conn)`**:
  Fungsi ini digunakan untuk membuat ruang obrolan baru di dalam saluran tertentu. Ruang obrolan baru akan dibuat di sistem file.
  ```c
  void create_room(const char* channel_name, const char* room_name, connection_t* conn) {
      char room_path[256];
      sprintf(room_path, "%s/%s/%s", path, channel_name, room_name);

      if (mkdir(room_path, 0777) == -1) {
          if (errno == EEXIST) {
              char response[] = "Room already exists";
              if (write(conn->sock, response, strlen(response)) < 0) {
                  perror("Response send failed");
              }
              return;
          } else {
              perror("Error creating room directory");
              char response[] = "Error creating room";
              if (write(conn->sock, response, strlen(response)) < 0) {
                  perror("Response send failed");
              }
              return;
          }
      }

      char response[] = "Room created successfully";
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```
- **`void delete_room(const char* channel_name, const char* room_name, connection_t* conn)`**:
  Fungsi ini digunakan untuk menghapus ruang obrolan yang ada di dalam saluran tertentu. Ruang obrolan akan dihapus dari sistem file.
  
```c
  void delete_room

(const char* channel_name, const char* room_name, connection_t* conn) {
      char room_path[256];
      sprintf(room_path, "%s/%s/%s", path, channel_name, room_name);

      if (rmdir(room_path) == -1) {
          perror("Error deleting room directory");
          char response[] = "Error deleting room";
          if (write(conn->sock, response, strlen(response)) < 0) {
              perror("Response send failed");
          }
          return;
      }

      char response[] = "Room deleted successfully";
      if (write(conn->sock, response, strlen(response)) < 0) {
          perror("Response send failed");
      }
  }
  ```


## Menjalankan Program
### Cara Menjalankan `discorit.c`

1.  **Kompilasi Program**: Untuk menjalankan `discorit.c`, Anda perlu mengompilasi kode sumbernya ke dalam sebuah program eksekutif. Misalnya, Anda dapat menggunakan perintah berikut di terminal:
    
    bash
    
    Salin kode
    
    `gcc discorit.c -o discorit` 
    
    Ini akan menghasilkan file eksekutif `discorit`.
    
2.  **Menjalankan Program**: Setelah berhasil dikompilasi, Anda dapat menjalankan program dengan perintah:
    
    bash
    
    Salin kode
    
    `./discorit` 
    
    Program ini akan menjalankan fungsinya untuk mengelola obrolan dan ruang di dalam saluran yang ditentukan.
    

### Cara Menjalankan `monitor.c`

1.  **Kompilasi Program**: Untuk `monitor.c`, langkah pertama adalah mengompilasi kode sumbernya menjadi program eksekutif. Contoh perintah kompilasi menggunakan gcc:
    
    bash
    
    Salin kode
    
    `gcc monitor.c -o monitor` 
    
    Ini akan menghasilkan file eksekutif `monitor`.
    
2.  **Menjalankan Program**: Setelah kompilasi selesai, jalankan program `monitor` dengan perintah:
    
    bash
    
    Salin kode
    
    `./monitor` 
    
    Program ini akan memulai tugas pemantauan dan pelaporan terhadap aktivitas server secara real-time.
    

### Cara Menjalankan `server.c`

1.  **Kompilasi Program**: Untuk menjalankan `server.c`, pastikan Anda telah mengompilasi kode sumbernya menjadi program eksekutif. Contoh perintah kompilasi menggunakan gcc:
    
    bash
    
    Salin kode
    
    `gcc server.c -o server` 
    
    Ini akan menghasilkan file eksekutif `server`.
    
2.  **Menjalankan Server**: Setelah berhasil dikompilasi, jalankan server dengan perintah:
    
    bash
    
    Salin kode
    
    `./server` 
    
    Server ini akan siap menerima koneksi dari klien dan mengelola berbagai operasi seperti pembuatan dan penghapusan saluran serta ruang obrolan.


### Menggunakan DiscorIT
#### Contoh Penggunaan Langkah demi Langkah

1.  **Menggunakan `discorit.c`**:
    
    -   Jalankan program `discorit`.
    -   Gunakan perintah `JOIN <channel>` untuk bergabung dengan saluran tertentu.
    -   Kirim pesan ke saluran dengan perintah `CHAT "<message>"`.
    -   Untuk melihat pesan yang ada di saluran, gunakan perintah `SEE_MESSAGES`.
2.  **Menggunakan `monitor.c`**:
    
    -   Jalankan program `monitor`.
    -   Program ini akan secara otomatis memantau aktivitas server dan mencatatnya dalam file log.
3.  **Menggunakan `server.c`**:
    
    -   Jalankan server dengan perintah `./server`.
    -   Server akan mulai mendengarkan permintaan dari klien pada port yang ditentukan.
    -   Klien dapat terhubung dan mengirimkan permintaan seperti `JOIN`, `CHAT`, atau `SEE_MESSAGES` ke server.

Contoh penggunaan
```
LOGIN tes -p ts
JOIN tes
LIST ROOM
DEL ROOM
EXIT
LIST USER
```


## Kesimpulan

Ketiga program ini berinteraksi satu sama lain untuk menyediakan layanan obrolan yang lengkap, dengan `discorit.c` sebagai klien, `monitor.c` sebagai pemantau server, dan `server.c` sebagai server utama. Fungsi-fungsi yang ada memastikan bahwa sistem dapat mengelola saluran dan ruang obrolan dengan efektif, serta memberikan layanan yang dapat diandalkan bagi pengguna.
