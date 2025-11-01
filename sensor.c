/* Guardar como: sensor.c */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>      // Para nanosleep
#include <fcntl.h>     // Para O_WRONLY
#include <unistd.h>    // Para write, close
#include <sys/stat.h>  // Para mkfifo (aunque lo hacemos por terminal)

#define FIFO_PATH "mi_pipe"
#define DATOS_FILE "datos.txt"

int main() {
    int fd_pipe;
    FILE *file;
    
    long tiempo_ns;
    float temp;
    struct timespec ts;

    // --- Abrir archivo de datos ---
    file = fopen(DATOS_FILE, "r");
    if (file == NULL) {
        perror("Error abriendo datos.txt");
        return 1;
    }

    // --- Abrir el pipe para escribir ---
    // Esto se bloqueará hasta que el monitor se conecte
    printf("Sensor: Esperando que el monitor se conecte a '%s'...\n", FIFO_PATH);
    fd_pipe = open(FIFO_PATH, O_WRONLY);
    if (fd_pipe == -1) {
        perror("Error abriendo el pipe");
        fclose(file);
        return 1;
    }
    
    printf("Sensor: ¡Monitor conectado! Empezando a enviar datos.\n");

    // --- Bucle principal: leer archivo y enviar datos ---
    while (fscanf(file, "%ld %f", &tiempo_ns, &temp) == 2) {
        
        // 1. Configurar el tiempo de espera
        ts.tv_sec = tiempo_ns / 1000000000;
        ts.tv_nsec = tiempo_ns % 1000000000;
        
        // 2. Dormir (simulando el tiempo de lectura del sensor)
        printf("Sensor: Esperando %ld ns...\n", tiempo_ns);
        nanosleep(&ts, NULL);

        // 3. Escribir la temperatura en el pipe
        printf("Sensor: ==> Enviando temperatura: %.1f°C\n", temp);
        if (write(fd_pipe, &temp, sizeof(float)) == -1) {
            perror("Error escribiendo en el pipe");
            break;
        }
    }

    printf("Sensor: Fin del archivo de datos. Cerrando.\n");
    
    // --- Limpieza ---
    fclose(file);
    close(fd_pipe);
    
    return 0;
}
