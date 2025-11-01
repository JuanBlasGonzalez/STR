#define _POSIX_C_SOURCE 200809L
#include <pthread.h>  // Para los threads
#include <stdio.h>    // Para printf
#include <stdlib.h>   // Para abort
#include <time.h>     // Para clock_gettime y nanosleep
#include <unistd.h>   // Para sleep (aunque no lo usemos, es común)

// --- Constantes del problema ---
#define NUM_THREADS 2
#define NUM_ITERACIONES 1000
// 100 veces/seg = 10ms por ciclo.
// 10ms = 10,000,000 nanosegundos.
#define PERIODO_NS (10000000) 

// --- Variable global para guardar resultados ---
// Usaremos un array. El thread 0 escribe en la pos 0, el thread 1 en la pos 1.
// Es 'long' porque acumulará nanosegundos y será un número muy grande.
long latencia_total[NUM_THREADS] = {0}; 

/**
 * @brief Función que ejecutarán los threads periódicos.
 * Mide la latencia (retraso) del scheduler.
 */
void *funcion_thread(void *arg) {
    // 1. Identificar qué thread soy (0 o 1)
    // El 'main' nos pasará un puntero a un entero (0 o 1)
    int id = *(int*)arg;
    
    // Estructuras para manejar el tiempo
    struct timespec tiempo_despertar, tiempo_actual, tiempo_inicio;
    long latencia_ns;

    // 2. Obtener el tiempo de inicio
    clock_gettime(CLOCK_MONOTONIC, &tiempo_inicio);
    
    // Copiamos el tiempo de inicio para calcular el primer despertar
    tiempo_despertar = tiempo_inicio;

    for (int i = 0; i < NUM_ITERACIONES; i++) {
        
        // 3. Calcular el próximo momento exacto para despertar
        // Añadimos el periodo (10ms) al último tiempo de despertar
        tiempo_despertar.tv_nsec += PERIODO_NS;

        // Manejar el "overflow" (si los nanos > 1 segundo)
        if (tiempo_despertar.tv_nsec >= 1000000000) {
            tiempo_despertar.tv_nsec -= 1000000000;
            tiempo_despertar.tv_sec++;
        }

        // 4. Dormir hasta ese momento exacto
        // TIMER_ABSTIME significa que 'tiempo_despertar' es una hora absoluta
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tiempo_despertar, NULL);

        // --- AQUI EL THREAD DESPIERTA ---
        
        // 5. Medir la latencia
        // (El tiempo que pasó desde que DEBÍA despertar vs. que DESPERTÓ)
        
        clock_gettime(CLOCK_MONOTONIC, &tiempo_actual);
        
        // Calculamos la diferencia en nanosegundos
        latencia_ns = (tiempo_actual.tv_sec - tiempo_despertar.tv_sec) * 1000000000L;
        latencia_ns += (tiempo_actual.tv_nsec - tiempo_despertar.tv_nsec);

        // 6. Acumular la latencia
        if(latencia_ns > 0) { // Solo acumulamos si se despertó tarde
            latencia_total[id] += latencia_ns;
        }
    }
    
    return NULL;
}


/**
 * @brief Función principal
 * Crea y gestiona los threads, luego imprime los resultados.
 */
int main() {
    pthread_t threads[NUM_THREADS]; // Array para IDs de threads
    int thread_ids[NUM_THREADS];    // Array para pasar los IDs (0 y 1)
    
    printf("Iniciando %d threads, cada uno con %d iteraciones...\n", NUM_THREADS, NUM_ITERACIONES);

    // --- Crear los threads ---
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i; // El ID que le pasaremos
        
        // Creamos el thread
        if (pthread_create(&threads[i], NULL, funcion_thread, (void*)&thread_ids[i])) {
            printf("Error creando el thread %d\n", i);
            abort();
        }
    }

    // --- Esperar a que terminen (Join) ---
    // El 'main' se bloquea aquí hasta que los threads finalicen
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL)) {
            printf("Error uniendo el thread %d\n", i);
            abort();
        }
    }

    printf("Todos los threads han finalizado.\n");

    // --- Calcular e imprimir resultados ---
    for (int i = 0; i < NUM_THREADS; i++) {
        double promedio_ns = (double)latencia_total[i] / NUM_ITERACIONES;
        double promedio_us = promedio_ns / 1000.0; // Convertir a microsegundos (más legible)
        
        printf("Thread %d: Latencia promedio = %.2f microsegundos (µs)\n", i, promedio_us);
    }

    return 0;
}
