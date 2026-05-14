/*
 * CSC220 Final Project - Motorcycle Dashboard Phase 3
 * Creates the base system for each variable of the motorcycle
 *
 * Kaleb McCraw, Sofia Nikolic | Last Modified April 30, 2026
 */



#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITER 2147483647 // So loops don't go on forever (2^31-1 = no limit)
#define SLEEP_LEN 100000 // How many microseconds to sleep for each loop
#define ACCEL 0.005 // Acceleration rate
#define DECEL 0.1 // Deceleration rate

sem_t subsysSems[4]; // Semaphores for each subsystem
sem_t ecuSems[6]; // Semaphores for each part of ECU
pthread_mutex_t scMutex; // mutex for shared motorcycle variables
pthread_mutex_t inMutex[11]; // mutex for shared input variables
// [W, S, C, A, D, Z, H, F, K, I, Q]

pthread_cond_t dashCond;
int subsysCompletion = 0;



struct Motorcycle {
    /*
     * Structure to track all variables
     * that the motorcycle needs.
     * cruising = whether cruise mode is on
     * headlights = whether headlights are on
     * refueling = whether vehicle is refueling
     * running = whether the motorcycle is running
     * distCurrent = current trip length (km)
     * distTotal = all-time distance traveled (km)
     * speed = motion (kmh)
     * temp = engine temperature (Celsius)
     * turn = turn signal (left, none, right, both)
     */
    bool cruising; // 0-1
    bool headlights; // 0-1
    bool refueling; // 0-1s
    bool running; // 0-1
    double distCurrent; // >=0
    double distTotal; // >=0
    double fuel; // 0-4.7
    double timeCurrent; // >=0
    double timeTotal; // >=0
    float speed; // >=0
    int rpm; // >=0
    int temp; // >=0
    int turn; // 0-3 (none, left, right, hazard)
    char rpmZone[8]; // STR
    char tempZone[8]; // STR
};
struct Motorcycle motorVars;



struct Inputs {
    /*
     * Structure to track which inputs
     * the user is currently pressing.
     * [W, S, C, A, D, Z, H, F, K, I, Q]
     */
    bool l[11];
};
struct Inputs motorInputs;



void refresh_dashboard(void(*print_dashboard)(void)) {
    /*
     * Function from Canvas which
     * clears the terminal when run
     */
    printf("\033[H\033[J");
    print_dashboard();
    fflush(stdout);
}



void print_dashboard() {
    /*
     * Function to display every variable
     * for the dashboard in the terminal.
     */
    char fuelStr[8] = "-------";
    for (int i = 0; i < (int)(7.5 * motorVars.fuel / 4.7); i++) {
        fuelStr[i] = '#';
    }

    printf("#========================================#");
    printf("\n|        McCraw/Nikolic Dashboard        |");
    printf("\n#========================================#");
    printf("\n|ENG %-s         %02d:%02d:%02d      %04d:%02d:%02d|", (motorVars.running) ? "ON " : "OFF",
        (int)(motorVars.timeCurrent / 3600), (int)((int)motorVars.timeCurrent % 3600 / 60), ((int)motorVars.timeCurrent % 60),
        (int)(motorVars.timeTotal / 3600), (int)((int)motorVars.timeTotal % 3600 / 60), ((int)motorVars.timeTotal % 60));
    printf("\n| ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |");
    printf("\n|SPD %03d kmh           RPM %05d %8s|", (int)motorVars.speed, motorVars.rpm, motorVars.rpmZone);
    printf("\n|FUEL E %-7s F %-c    TMP %03d°C %8s|", fuelStr, (motorVars.fuel <= 0.7) ? '!' : ' ', motorVars.temp, motorVars.tempZone);
    printf("\n| ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |");
    printf("\n|TRIP %06.1f km         TOTAL %08.1f km|", motorVars.distCurrent, motorVars.distTotal);
    printf("\n|TURN SIGNAL %c             HEADLIGHTS %3s|",
        (motorVars.turn == 0) ? '_' : (motorVars.turn == 1) ? '<' : (motorVars.turn == 2) ? '>' : '!',
        (motorVars.headlights) ? "ON " : "OFF");
    printf("\n#========================================#");
}



void *input_reader() {
    /*
     * Function to read and pass inputs to subsystems; code used from
     * https://www.reddit.com/r/C_Programming/comments/15jytqt/comment/jv2qyao/
     */
    struct termios attr;
    tcgetattr(STDIN_FILENO, &attr);
    attr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);

    uint8_t buff[20];
    ssize_t bytes;
    int i = 0;
    while ((bytes = read(STDIN_FILENO, buff, 20)) > 0 && i++ < MAX_ITER) {
        for (size_t j = 0; j < bytes; j++) {
            if (buff[j] == 119) { // W: toggle
                motorInputs.l[0] = !motorInputs.l[0];
            }
            else if (buff[j] == 115) { // S: toggle
                motorInputs.l[1] = !motorInputs.l[1];
            }
            else if (buff[j] == 99) { // C: toggle
                motorInputs.l[2] = !motorInputs.l[2];
            }
            else if (buff[j] == 97) { // A: toggle
                motorInputs.l[3] = !motorInputs.l[3];
            }
            else if (buff[j] == 100) { // D: toggle
                motorInputs.l[4] = !motorInputs.l[4];
            }
            else if (buff[j] == 122) { // Z: toggle
                motorInputs.l[5] = !motorInputs.l[5];
            }
            else if (buff[j] == 104) { // H: toggle
                motorInputs.l[6] = !motorInputs.l[6];
            }
            else if (buff[j] == 102) { // F: button
                pthread_mutex_lock(&inMutex[7]);
                motorInputs.l[7] = true;
                pthread_mutex_unlock(&inMutex[7]);
            }
            else if (buff[j] == 107) { // K: button
                pthread_mutex_lock(&inMutex[8]);
                motorInputs.l[8] = true;
                pthread_mutex_unlock(&inMutex[8]);
            }
            else if (buff[j] == 105) { // I: button
                pthread_mutex_lock(&inMutex[9]);
                motorInputs.l[9] = true;
                pthread_mutex_unlock(&inMutex[9]);
            }
            else if (buff[j] == 113) { // Q: button (absolute)
                motorInputs.l[10] = true;
                for (int i = 0; i < 4; i ++) {
                    sem_post(&subsysSems[i]);
                    sem_post(&ecuSems[i]);
                }
                pthread_mutex_lock(&scMutex);
                subsysCompletion++;
                pthread_cond_broadcast(&dashCond);
                pthread_mutex_unlock(&scMutex);
                return NULL;
            }
        }
    }
    return NULL;
}



int maxSpeed = 200; // speed cap
void *sys_motion() {
    /*
     * Function to update speed
     * and distance variables
     */
    srand(time(NULL));
    motorVars.distTotal = rand() % 10000;

    int i = 0;
    double adTime[2] = {0.0, 0.0};
    while (i++ < MAX_ITER) {
        sem_wait(&subsysSems[0]);
        sem_post(&ecuSems[5]);
        if (motorInputs.l[10]) return NULL;

        if (motorVars.running) {
            const double INTERVALDIST = (motorVars.speed / 3600.0) * (SLEEP_LEN / 1e6);
            motorVars.distCurrent += INTERVALDIST;
            motorVars.distTotal += INTERVALDIST;
            
            if (motorInputs.l[2]) {
                motorVars.cruising = !motorVars.cruising;
            }
            if (!motorVars.cruising) {
                if (motorInputs.l[0] && motorVars.speed < maxSpeed) {
                    motorVars.speed += (maxSpeed - motorVars.speed) * ACCEL * adTime[0];
                    adTime[0] += (double)SLEEP_LEN/1e6;
                    if (adTime[1] != 0.0) adTime[1] = 0.0;
                }
                else if ((motorInputs.l[1] || motorVars.speed > maxSpeed) && motorVars.speed > 0) {
                    motorVars.speed -= DECEL * adTime[1];
                    adTime[1] += (double)SLEEP_LEN/1e6;
                    if (adTime[0] != 0.0) adTime[0] = 0.0;
                }
                else {
                    if (adTime[0] != 0.0) adTime[0] = 0.0;
                    if (adTime[1] != 0.0) adTime[1] = 0.0;
                }
                if (motorVars.speed < 0) motorVars.speed = 0;
            }

            sem_post(&subsysSems[1]);
        }
        else if (motorVars.speed > 0) {
            const double INTERVALDIST = (motorVars.speed / 3600.0) * (SLEEP_LEN / 1e6);
            motorVars.distTotal += INTERVALDIST;
            if (motorVars.speed == 0) motorVars.distCurrent = 0.0;

            motorVars.speed -= DECEL * adTime[1];
            adTime[1] += (double)SLEEP_LEN/1e6;
            if (adTime[0] != 0.0) adTime[0] = 0.0;
            if (motorVars.speed < 0) motorVars.speed = 0;
            sem_post(&subsysSems[1]);
        }
        else {
            sem_post(&subsysSems[1]);
            motorVars.distCurrent = 0.0;
        }
        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_engine() {
    /*
     * Function to update RPM and temp
     * variables according to speed
     */
    int i = 0, variance = 0, add = 10;
    while (i++ < MAX_ITER) {
        sem_wait(&subsysSems[1]);
        if (motorInputs.l[10]) return NULL;

        if (motorVars.running) {
            // RPM is 60 times the speed (kmh), + 1000
            if (abs(motorVars.rpm - (60 * (int)motorVars.speed + 1100)) > 500) { // if the RPM isn't right, slowly correct
                motorVars.rpm += (motorVars.rpm < 60 * (int)motorVars.speed + 1100) ? 750 : -750;
                sem_post(&ecuSems[1]);
            }
            else {
                motorVars.rpm = 60 * (int)motorVars.speed + 1100 + variance;
                sem_post(&ecuSems[1]);

                variance += add;
                if (variance >= 190) add = -10;
                if (variance <= 0) add = 10;
            }
            // Temp is 1/200th of the RPM, + 50
            if (abs((motorVars.temp - 50) * 200 - motorVars.rpm) > 300) motorVars.temp += 1;
            else motorVars.temp = motorVars.rpm / 200 + 50;
            sem_post(&ecuSems[2]);
            sem_post(&subsysSems[2]);
        }
        else if ((motorVars.rpm > 0) || (motorVars.temp != 30)) {
            motorVars.rpm -= (motorVars.rpm > 0) ? 100 : motorVars.rpm;
            if (motorVars.speed > 0) motorVars.rpm = 60 * (int)motorVars.speed + 1100;
            sem_post(&ecuSems[1]);

            if (motorVars.rpm > 0) motorVars.temp = motorVars.rpm / 200 + 50;
            else {
                if (motorVars.temp > 30) motorVars.temp -= 1;
                else motorVars.temp = 30;
            }

            if (motorVars.rpm < 0) motorVars.rpm = 0;
            sem_post(&ecuSems[2]);
            sem_post(&subsysSems[2]);
        }
        else {
            sem_post(&ecuSems[1]);
            sem_post(&ecuSems[2]);
            sem_post(&subsysSems[2]);
        }
    }
    return NULL;
}

void *sys_fuel() {
    /*
     * Function to update fuel
     * amount based on rpm
     */
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&subsysSems[2]);
        if (motorInputs.l[10]) return NULL;
        // Fuel depletes at 10e-6 times the rpm per second
        if (motorVars.running) motorVars.fuel -= motorVars.rpm / (1e12 / SLEEP_LEN);
        if (motorVars.refueling) motorVars.fuel += 0.1;
        if (motorVars.fuel > 4.7) motorVars.fuel = 4.7;

        sem_post(&ecuSems[3]);
        sem_post(&subsysSems[3]);
    }
    return NULL;
}



void *sys_ecu_time() {
    /*
     * Function to update time within ECU.
     */
    srand(time(NULL));
    int storedTime = rand() % 86400;
    motorVars.timeTotal = storedTime;

    struct timespec timeStart;
    clock_gettime(CLOCK_MONOTONIC, &timeStart);

    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[0]);
        if (motorInputs.l[10]) return NULL;

        if (motorVars.running) {
            struct timespec timeNow;
            clock_gettime(CLOCK_MONOTONIC, &timeNow);

            motorVars.timeCurrent = ((double)timeNow.tv_sec + 1e-9*timeNow.tv_nsec)
                            - ((double)timeStart.tv_sec + 1e-9*timeStart.tv_nsec);
            motorVars.timeTotal = storedTime + motorVars.timeCurrent;
        }
        else {
            if (motorVars.timeCurrent != 0.0) motorVars.timeCurrent = 0.0;
            clock_gettime(CLOCK_MONOTONIC, &timeStart);
            storedTime = motorVars.timeTotal;
        }
        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu_rpm() {
    /*
     * Function to update rpm zone within ECU.
     */
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[1]);
        if (motorInputs.l[10]) return NULL;

        const char *rz = (motorVars.rpm < 1100) ? "OFF\0" :
                         (motorVars.rpm < 1300) ? "IDLE\0" :
                         (motorVars.rpm < 8000) ? "NORMAL\0" :
                         (motorVars.rpm < 14500) ? "HIGH\0" : "REDLINE\0";
        strcpy(motorVars.rpmZone, rz);

        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu_temp() {
    /*
     * Function to update temp zone within ECU.
     */
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[2]);
        sem_post(&ecuSems[4]);
        if (motorInputs.l[10]) return NULL;

        const char *tz = (motorVars.temp < 60) ? "COLD\0" :
                         (motorVars.temp < 95) ? "NORMAL\0" :
                         (motorVars.temp < 105) ? "HOT\0" : "OVERHEAT\0";
        strcpy(motorVars.tempZone, tz);

        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu_fuel() {
    /*
     * Function to update fuel stuff within ECU.
     */
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[3]);
        if (motorInputs.l[10]) return NULL;

        if (motorVars.fuel <= 0) motorVars.running = false;

        if ((motorVars.fuel <= 0.7 || motorVars.temp >= 105) && maxSpeed > 100) maxSpeed = 100;
        else if (motorVars.fuel > 0.7 && motorVars.temp < 95 && maxSpeed <= 100 && motorVars.speed <= maxSpeed) maxSpeed = 200;

        if (motorInputs.l[7] && !motorVars.running && motorVars.speed == 0.0) motorVars.refueling = true;
        if (motorVars.refueling && motorVars.fuel == 4.7) motorVars.refueling = false;
        // CRITICAL SECTION: toggle refuel input
        pthread_mutex_lock(&inMutex[7]);
        motorInputs.l[7] = false;
        pthread_mutex_unlock(&inMutex[7]);

        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu_state() {
    /*
     * Function to update engine state within ECU.
     */
    enum State {
        ENGINE_OFF,
        IDLE,
        NORMAL,
        HIGH_LOAD,
        CRITICAL
    };
    enum State engineState = ENGINE_OFF;

    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[4]);
        if (motorInputs.l[10]) return NULL;

        if (motorInputs.l[8] && motorVars.running) motorVars.running = false;
        else if (motorInputs.l[9] && !motorVars.running && !motorVars.refueling &&
            motorVars.speed == 0.0 && motorVars.fuel > 0) motorVars.running = true;
        // CRITICAL SECTIONS: turning off input buttons for killswitch/ignition
        pthread_mutex_lock(&inMutex[8]);
        motorInputs.l[8] = false;
        pthread_mutex_unlock(&inMutex[8]);
        pthread_mutex_lock(&inMutex[9]);
        motorInputs.l[9] = false;
        pthread_mutex_unlock(&inMutex[9]);

        engineState = (motorVars.rpm + 200 * motorVars.temp < 12000) ? ENGINE_OFF :
                      (motorVars.rpm + 200 * motorVars.temp < 13000) ? IDLE :
                      (motorVars.rpm + 200 * motorVars.temp < 27000) ? IDLE :
                      (motorVars.rpm + 200 * motorVars.temp < 35000) ? HIGH_LOAD : CRITICAL;
        
        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu_misc() {
    /*
     * Function to update miscellaneous functions with ECU.
     */
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_wait(&ecuSems[5]);
        if (motorInputs.l[10]) return NULL;

        motorVars.headlights = motorInputs.l[6];
        if (motorInputs.l[5]) motorVars.turn = 3;
        else if (motorInputs.l[3]) motorVars.turn = 1;
        else if (motorInputs.l[4]) motorVars.turn = 2;
        else motorVars.turn = 0;
        
        // CRITICAL SECTION: Iterates subsystem counter
        pthread_mutex_lock(&scMutex);
        subsysCompletion++;
        pthread_cond_broadcast(&dashCond);
        pthread_mutex_unlock(&scMutex);
    }
    return NULL;
}

void *sys_ecu() {
    /*
     * Function to format motorcycle
     * data and keep track of time
     * via four subthreads
     */
    for (int i = 0; i < 6; i++) {
        sem_init(&ecuSems[i], 0, 0);
    }

    // Allocate work to subthreads so it can update faster
    pthread_t ecuThreads[6];
    pthread_cond_init(&dashCond, NULL);

    pthread_create(&ecuThreads[0], NULL, sys_ecu_time, NULL);
    pthread_create(&ecuThreads[1], NULL, sys_ecu_rpm, NULL);
    pthread_create(&ecuThreads[2], NULL, sys_ecu_temp, NULL);
    pthread_create(&ecuThreads[3], NULL, sys_ecu_fuel, NULL);
    pthread_create(&ecuThreads[4], NULL, sys_ecu_state, NULL);
    pthread_create(&ecuThreads[5], NULL, sys_ecu_misc, NULL);

    for (int i = 0; i < 6; i++) {
        pthread_join(ecuThreads[i], NULL);
    }

    return NULL;
}



void *update_dashboard() {
    /*
     * Function to update the dashboard
     * by clearing and printing in console
     */

    sem_post(&subsysSems[0]);
    int i = 0;
    while (i++ < MAX_ITER) {
        sem_post(&ecuSems[0]);

        // CRITICAL SECTION: Checks and writes subsystem counter 
        pthread_mutex_lock(&scMutex);
        int j = 0;
        while (subsysCompletion < 7 && j++ < MAX_ITER) {
            pthread_cond_wait(&dashCond, &scMutex);
        }
        if (motorInputs.l[10]) return NULL;
        refresh_dashboard(print_dashboard);
        
        subsysCompletion = 0;
        pthread_mutex_unlock(&scMutex);

        sem_post(&subsysSems[0]);
        usleep(SLEEP_LEN);
    }
    return NULL;
}



int stop() {
    /*
     * Function to delete every semaphore
     * and mutex var to stop the program
     */
    pthread_mutex_destroy(&scMutex);
    pthread_cond_destroy(&dashCond);
    for (int i = 0; i < 4; i++) {
        sem_destroy(&subsysSems[i]);
        sem_destroy(&ecuSems[i]);
        pthread_mutex_destroy(&inMutex[i]);
    }
    for (int i = 4; i < 6; i++) {
        sem_destroy(&ecuSems[i]);
        pthread_mutex_destroy(&inMutex[i]);
    }
    for (int i = 6; i < 11; i++) pthread_mutex_destroy(&inMutex[i]);
    printf("\n");
    return 0;
}

struct termios OriginalTerminos;
int main() {
    tcgetattr(STDIN_FILENO, &OriginalTerminos);
    // Create and join threads + mutex
    for (int i = 0; i < 5; i++) {
        sem_init(&subsysSems[i], 0, 0);
    }
    pthread_t threads[6];
    pthread_mutex_init(&scMutex, NULL);
    
    pthread_create(&threads[0], NULL, sys_engine, NULL);
    pthread_create(&threads[1], NULL, sys_motion, NULL);
    pthread_create(&threads[2], NULL, sys_fuel, NULL);
    pthread_create(&threads[3], NULL, sys_ecu, NULL);
    pthread_create(&threads[4], NULL, update_dashboard, NULL);
    pthread_create(&threads[5], NULL, input_reader, NULL);

    for (int i = 0; i < 6; i++) {
        pthread_join(threads[i], NULL);
    }
    
    stop();
    tcsetattr(STDIN_FILENO, TCSANOW, &OriginalTerminos);
    return 0;
}
