/* Guardar como: monitor.c */

// Importante: define el estándar POSIX para habilitar las funciones de tiempo real
#define _POSIX_C_SOURCE 200112L 

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>    // Para read, close
#include <fcntl.h>     // Para O_RDONLY
#include <time.h>      // Para nanosleep
#include <string.h>    // Para stderr
#include <sched.h>     // Para políticas de scheduling (sched_...)

#define FIFO_PATH "mi_pipe"

// --- Variables Globales para comunicar los threads ---
float temperatura_compartida;
int   hay_dato_nuevo = 0; // 0=No, 1=Si, -1=Terminar
pthread_mutex_t mutex_dato = PTHREAD_MUTEX_INITIALIZER;

// --- Variables para el promedio ---
float ultimas_3_temps[3] = {0.0, 0.0, 0.0};
int   indice_temps = 0;


/**
 * @brief THREAD ALTA PRIORIDAD (SCHED_FIFO)
 * Tarea: Leer del pipe, verificar alarma y pasar el dato.
 */
void *funcion_thread_alta(void *arg) {
    int fd_pipe;
    float temp_leida;

    printf("Monitor [ALTA]: Abriendo pipe '%s' para leer...\n", FIFO_PATH);
    fd_pipe = open(FIFO_PATH, O_RDONLY);
    if (fd_pipe == -1) {
        perror("Monitor [ALTA]: Error abriendo pipe");
        return NULL;
    }
    
    printf("Monitor [ALTA]: Pipe abierto. Esperando datos...\n");

    // Bucle: se bloquea en 'read' hasta que llega un dato
    while (read(fd_pipe, &temp_leida, sizeof(float)) > 0) {
        
        printf("Monitor [ALTA]: Dato recibido: %.1f°C\n", temp_leida);
        
        // 1. Verificar alarma
        if (temp_leida > 90.0) {
            printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            printf("Monitor [ALTA]: ALERTA! Temperatura %.1f°C excede los 90°C!\n", temp_leida);
            printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        } else {
            // 2. Pasar el dato al thread de baja prioridad
            pthread_mutex_lock(&mutex_dato);
            temperatura_compartida = temp_leida;
            hay_dato_nuevo = 1;
            pthread_mutex_unlock(&mutex_dato);
        }
    }

    printf("Monitor [ALTA]: El sensor cerró el pipe. Terminando.\n");
    close(fd_pipe);
    
    // Señal para que el thread de baja prioridad también termine
    pthread_mutex_lock(&mutex_dato);
    hay_dato_nuevo = -1; // Señal de fin
    pthread_mutex_unlock(&mutex_dato);
    
    return NULL;
}


/**
 * @brief THREAD BAJA PRIORIDAD (SCHED_RR)
 * Tarea: Calcular y mostrar el promedio de los últimos 3 valores.
 */
void *funcion_thread_baja(void *arg) {
    // Tiempo de espera para no consumir 100% CPU (10ms)
    struct timespec ts_sleep = {0, 10000000L};
    int dato_flag;

    printf("Monitor [BAJA]: Iniciado. Esperando datos...\n");
    
    while (1) {
        pthread_mutex_lock(&mutex_dato);
        dato_flag = hay_dato_nuevo; // Copio el flag
        
        if (dato_flag == 1) { // Hay dato nuevo
            // Consumo el dato
            float temp_actual = temperatura_compartida;
            hay_dato_nuevo = 0; // Apago el flag
            
            // Libero el mutex ANTES de hacer el cálculo
            pthread_mutex_unlock(&mutex_dato);

            // --- Procesar el dato (fuera de la zona crítica) ---
            ultimas_3_temps[indice_temps] = temp_actual;
            indice_temps = (indice_temps + 1) % 3; // Índice circular (0, 1, 2, 0, ...)
            
            float promedio = (ultimas_3_temps[0] + ultimas_3_temps[1] + ultimas_3_temps[2]) / 3.0;
            printf("Monitor [BAJA]: Promedio 3 últ: %.1f°C\n", promedio);

        } else if (dato_flag == -1) { // Señal de fin
            pthread_mutex_unlock(&mutex_dato);
            break; // Salir del bucle while
        } else {
            // No hay dato, solo suelto el mutex
            pthread_mutex_unlock(&mutex_dato);
        }
        
        // Dormir un poco para ceder la CPU
        nanosleep(&ts_sleep, NULL);
    }
    
    printf("Monitor [BAJA]: Terminando.\n");
    return NULL;
}


/**
 * @brief FUNCION PRINCIPAL
 * Tarea: Configurar y lanzar los threads con sus prioridades.
 */
int main() {
    pthread_t tid_alta, tid_baja;
    pthread_attr_t attr_alta, attr_baja;
    struct sched_param param_alta, param_baja;

    // --- Inicializar atributos ---
    pthread_attr_init(&attr_alta);
    pthread_attr_init(&attr_baja);

    // --- Establecer que usaremos la configuración explícita ---
    // (Sin esto, ignoraría las políticas que seteamos)
    pthread_attr_setinheritsched(&attr_alta, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&attr_baja, PTHREAD_EXPLICIT_SCHED);

    // --- Obtener prioridades ---
    int prio_max = sched_get_priority_max(SCHED_FIFO);
    int prio_min = sched_get_priority_min(SCHED_RR);
    
    printf("Prioridad MAX (FIFO): %d\n", prio_max);
    printf("Prioridad MIN (RR):   %d\n", prio_min);

    // --- Configurar Thread ALTA (FIFO) ---
    pthread_attr_setschedpolicy(&attr_alta, SCHED_FIFO);
    param_alta.sched_priority = prio_max;
    if (pthread_attr_setschedparam(&attr_alta, &param_alta) != 0) {
        fprintf(stderr, "Error seteando prioridad ALTA\n");
    }

    // --- Configurar Thread BAJA (RR) ---
    pthread_attr_setschedpolicy(&attr_baja, SCHED_RR);
    param_baja.sched_priority = prio_min;
     if (pthread_attr_setschedparam(&attr_baja, &param_baja) != 0) {
        fprintf(stderr, "Error seteando prioridad BAJA\n");
    }

    // --- Crear los threads ---
    // (Necesitás sudo para que pthread_create acepte SCHED_FIFO/RR)
    printf("Iniciando threads... (Requiere 'sudo')\n");
    
    if (pthread_create(&tid_alta, &attr_alta, funcion_thread_alta, NULL) != 0) {
        perror("Error creando thread ALTA");
        return 1;
    }
    if (pthread_create(&tid_baja, &attr_baja, funcion_thread_baja, NULL) != 0) {
        perror("Error creando thread BAJA");
        return 1;
    }

    // --- Esperar a que los threads terminen ---
    pthread_join(tid_alta, NULL);
    pthread_join(tid_baja, NULL);

    // --- Limpieza ---
    pthread_attr_destroy(&attr_alta);
    pthread_attr_destroy(&attr_baja);
    pthread_mutex_destroy(&mutex_dato);
    
    printf("Monitor: Todos los threads terminaron. Saliendo.\n");
    
    return 0;
}
