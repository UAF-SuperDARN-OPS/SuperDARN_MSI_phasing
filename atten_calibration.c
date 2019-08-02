#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifdef __QNX__
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#endif

#include "registers.h"
#include "include/common_functions.h"

#define SWITCHES 0
#define ATTEN    1
#define READ     0
#define WRITE    1
#define ON       1
#define OFF      0

#define NEW_PMAT 1

#define CARDS 200
#define ATTENCODES 64
#define FREQS 1500
#define _QUICK_

int stupid_flag = 0;
int setup_flag = 1;
int test_flag = -1000;
int sock = -1;
int verbose = 2;
char *hostip = "192.168.1.2";
char *file_prefix = "phasing_cal";
char *file_ext = ".dat";
char *atten_ext = ".att";
char attenfilename[120];
char *dir = "/data/cal/";
FILE *attenfile = NULL;
int port = 23;
char command[80];
char radar_name[80];
char freq_start[10] = "8E6";
char freq_stop[10] = "20E6";
char freq_steps[10] = "201";

struct timeval t0, t1, t2, t3, t4, t5, t6;
struct timeval t10, t11;
unsigned long elapsed;

int mlog_data_command(char *command, double *array[FREQS], int b) {
    int32 count, rval, sample_count;
    char output[10] = "";
    char command2[80];
    char cmd_str[80], prompt_str[10], data_str[1000];
    int32 cr, lf;
    strcpy(command2, command);
    if (verbose > 2) printf("%d Command: %s\n", strlen(command2), command2);
    write(sock, &command2, sizeof(char) * strlen(command2));
    cr = 0;
    lf = 0;
    count = 0;
    if (verbose > 2) fprintf(stdout, "Command Output String::\n");
    strcpy(cmd_str, "");
    while ((cr == 0) || (lf == 0)) {
        rval = read(sock, &output, sizeof(char) * 1);
#ifdef __QNX__
        if (rval<1) delay(1);
#else
        if (rval < 1) {
            usleep(10);

        }
#endif
        if (output[0] == 13) {
            cr++;
            continue;
        }
        if (output[0] == 10) {
            lf++;
            continue;
        }
        count += rval;
        strncat(cmd_str, output, rval);
        if (verbose > 2) fprintf(stdout, "%c", output[0]);
    }
    if (verbose > 2) printf("Processing Data\n");

    cr = 0;
    lf = 0;
    count = 0;
    sample_count = 0;
    if (verbose > 2) fprintf(stdout, "\nData Output String::\n");
    strcpy(data_str, "");
    if (verbose > 2) fprintf(stdout, "%d: ", sample_count);
    while ((cr == 0) || (lf == 0)) {
        rval = read(sock, &output, sizeof(char) * 1);
        if (output[0] == 13) {
            cr++;
            continue;
        }
        if (output[0] == 10) {
            lf++;
            continue;
        }
        if (output[0] == ',') {
            if ((sample_count % 2) == 0) {
                if (sample_count / 2 >= FREQS) {
                    printf("ERROR: too many samples... aborting\n");
                    exit(-1);
                }
                array[sample_count / 2][b] = atof(data_str);
                if (verbose > 2) fprintf(stdout, "%s  ::  %lf", data_str, array[sample_count / 2][b]);
            }
            sample_count++;
            if (verbose > 2) fprintf(stdout, "\n%d: ", sample_count);
            strcpy(data_str, "");
        } else {
            strncat(data_str, output, rval);
        }
    }
    if ((sample_count % 2) == 0) {
        if (sample_count / 2 >= FREQS) {
            printf("ERROR: too many samples... aborting\n");
            exit(-1);
        }
        array[sample_count / 2][b] = atof(data_str);
        if (verbose > 2) fprintf(stdout, "%s  ::  %lf", data_str, array[sample_count / 2][b]);
    }
    sample_count++;
    strcpy(data_str, "");
    if (verbose > 2) fprintf(stdout, "\nSamples: %d\n", sample_count / 2);
    if (verbose > 2) fprintf(stdout, "\nPrompt String::\n");
    while (output[0] != '>') {
        rval = read(sock, &output, sizeof(char) * 1);
#ifdef __QNX__
        if (rval<1) delay(1);
#else
        if (rval < 1) usleep(10);
#endif
        strncat(prompt_str, output, rval);
        if (verbose > 2) fprintf(stdout, "%c", output[0]);
    }
    return 0;
}

int button_command(char *command) {
    int count, rval;
    char output[10] = "";
    char command2[80];
    char prompt_str[80];
/*
*  Process Command String with No feedback 
*/
    strcpy(command2, command);
    if (verbose > 2) fprintf(stdout, "%d Command: %s\n", strlen(command2), command2);
    write(sock, &command2, sizeof(char) * strlen(command2));
    count = 0;
    if (verbose > 2) fprintf(stdout, "\nPrompt String::\n");
    while (output[0] != '>') {
        rval = read(sock, &output, sizeof(char) * 1);
        strncat(prompt_str, output, rval);
        if (verbose > 2) fprintf(stdout, "%c", output[0]);
        count++;
    }
    if (verbose > 2) fprintf(stdout, "Command is done\n", command2);
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv) {
    char output[40], strout[40];
    char cmd_str[80], prompt_str[10], data_str[1000];
    double *atten_phase[FREQS], *atten_pwr_mag[FREQS];
    double freq[FREQS];
    double pd_old, pd_new, pwr_diff = 0.0;
    int32 rval, count, sample_count, fail, cr, lf;
    int32 ii, i = 0, c = 31, data = 0, index = 0, wait_delay = 10;
    unsigned int b = 0;
    int last_collect, current_collect, collect = 0, beamcode = 0, take_data = 0, attempt = 0, max_attempts = 20;
    double fstart;
    double fstop;
    double fstep;
    int fnum;
    int radar;
    char serial_number[80];
    unsigned int portA0, portB0, portC0, cntrl0;
    unsigned int portA1, portB1, portC1, cntrl1;
    int temp, pci_handle, j, IRQ;
    unsigned char *BASE0, *BASE1;
    unsigned int mmap_io_ptr, IOBASE, CLOCK_RES;
    float time;
#ifdef __QNX__
    struct		 _clockperiod new, old;
    struct		 timespec start_p, stop_p, start, stop, nsleep;

    if(argc <2 ) {
      fprintf(stderr,"%s: invoke with radar number (1 or 2 or 3)\n",argv[0]);
      fflush(stderr);
      exit(0);
    }
    if(argc ==3 ) {
      if(atoi(argv[2])==0) setup_flag=0;
      else setup_flag=1;
    } else {
      test_flag=-1000;
    }
    if(argc ==4 ) {
      test_flag=atoi(argv[3]);
      c=atoi(argv[2]);
    } else {
      test_flag=-1000;
    }
    radar=atoi(argv[1]);
    printf("Radar: %d Card: %d\n",radar,c);
    printf("Test flag: %d\n",test_flag);
    switch(radar) {
      case 1:
            portC0=PC_GRP_0;
            portC1=PC_GRP_1;
            portB0=PB_GRP_0;
            portB1=PB_GRP_1;
            portA0=PA_GRP_0;
            portA1=PA_GRP_1;
            cntrl0=CNTRL_GRP_0;
            cntrl1=CNTRL_GRP_1;
        break;
      case 2:
            portC0=PC_GRP_2;
            portC1=PC_GRP_3;
            portB0=PB_GRP_2;
            portB1=PB_GRP_3;
            portA0=PA_GRP_2;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_2;
            cntrl1=CNTRL_GRP_3;
        break;
      case 3:
            portC0=PC_GRP_4;
            portC1=PC_GRP_3;
            portB0=PB_GRP_4;
            portB1=PB_GRP_3;
            portA0=PA_GRP_4;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_4;
            cntrl1=CNTRL_GRP_3;
        break;
      default:
        fprintf(stderr,"Invalid radar number %d",radar);
        exit(-1);
    }
    /* SET THE SYSTEM CLOCK RESOLUTION AND GET THE START TIME OF THIS PROCESS */
    new.nsec=10000;
    new.fract=0;
    temp=ClockPeriod(CLOCK_REALTIME,&new,0,0);
    if(temp==-1){
        perror("Unable to change system clock resolution");
    }
    temp=clock_gettime(CLOCK_REALTIME, &start_p);
    if(temp==-1){
        perror("Unable to read sytem time");
    }
    temp=ClockPeriod(CLOCK_REALTIME,0,&old,0);
    CLOCK_RES=old.nsec;
    printf("CLOCK_RES: %d\n", CLOCK_RES);
    /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
    fprintf(stderr,"PLX9052 CONFIGURATION ********************\n");
    clock_gettime(CLOCK_REALTIME, &start);
    temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 1);
    IOBASE=mmap_io_ptr;
    if(temp==-1){
        fprintf(stderr, "	PLX9052 configuration failed");
    }
    else{
        fprintf(stderr, "	PLX9052 configuration successful!\n");
    }
    printf("IOBASE=%x\n",IOBASE);
    /* INITIALIZE THE CARD FOR PROPER IO */
    // GROUP 0 - PortA=output, PortB=output, PortClo=output, PortChi=output
    out8(IOBASE+cntrl0,0x80);
    // GROUP 1 - PortAinput, PortB=input, PortClo=input, PortChi=output
    out8(IOBASE+cntrl1,0x93);
    out8(IOBASE+portA0,0x00);
    out8(IOBASE+portB0,0x00);
    out8(IOBASE+portC0,0x00);
    out8(IOBASE+portA1,0x00);
    out8(IOBASE+portB1,0x00);
    out8(IOBASE+cntrl0,0x00);
    out8(IOBASE+cntrl1,0x13);
    temp=in8(IOBASE+portC1);
    temp=temp & 0x0f;
    printf("input on group 1, port c is %x\n", temp);
        select_card(IOBASE,c,radar);
/*
        while(1) { 
        printf("Set Write\n");
          set_RW(IOBASE,WRITE,radar);
          sleep(1);
        printf("Set Read\n");
          set_RW(IOBASE,READ,radar);
          sleep(1);
        printf("Set switches \n");
          set_SA(IOBASE,SWITCHES,radar);
          sleep(1);
        printf("Set atten\n");
          set_SA(IOBASE,ATTEN,radar);
          sleep(1);
        }
        exit(0);
*/
#endif
    if (test_flag == -3) {
        while (1) {
            for (i = 0; i <= 12; i = i + 1) {
                //     b=0;
                //     printf("Selecting Beamcode: %d 0x%x\n",b,b);
                //     beam_code(IOBASE,b,1);
                //     sleep(1);
                if (i == -1) b = 0;
                if (i == 0) b = 1;
                if (i > 0) b = 1 << i;
                b = b | 0x200;
//        b= (b |  0x4);
                printf("Selecting Beamcode: %d 0x%x\n", b, b);
                beam_code(IOBASE, b, 1);
                sleep(1);
            }
        }
    }
    if (test_flag >= 0) {
        //stupid_flag=1;
        //printf("test flag %d radar %d\n",test_flag,radar);
        beam_code(IOBASE, test_flag, radar);
        //      temp=write_data_new(IOBASE,c,test_flag,test_flag,radar,0);
        if (NEW_PMAT) temp = verify_data_new(IOBASE, c, test_flag, test_flag, radar, 1);
        else temp = verify_data_old(IOBASE, c, test_flag, test_flag, radar, 1);
        exit(0);
    }
    if (test_flag == -2) {
        printf("test flag %d radar %d icard %d verify programming\n", test_flag, radar, c);
        for (b = 0; b <= 8191; b++) {
//    beam_code(IOBASE,b,radar);
            usleep(10000);
            if (NEW_PMAT) temp = verify_data_new(IOBASE, c, b, b, radar, 0);
            else temp = verify_data_old(IOBASE, c, b, b, radar, 1);
        }
        exit(0);
    }
/*
if(test_flag==1) {
  beam_code(IOBASE,8191,radar);
  exit(0);
}
*/
    fnum = atoi(freq_steps);
    fstart = atof(freq_start);
    fstop = atof(freq_stop);
    fstep = (fstop - fstart) / (fnum - 1);
    for (i = 0; i < fnum; i++) {
        freq[i] = fstart + i * fstep;
        atten_phase[i] = calloc(ATTENCODES, sizeof(double));
        atten_pwr_mag[i] = calloc(ATTENCODES, sizeof(double));
    }
    if (test_flag == -1000) {
// Open Socket and initial IO
        if (verbose > 0) printf("Opening Socket %s %d\n", hostip, port);
        sock = opentcpsock(hostip, port);
        if (sock < 0) {
            if (verbose > 0) printf("Socket failure %d\n", sock);
        } else if (verbose > 0) printf("Socket %d\n", sock);
        rval = read(sock, &output, sizeof(char) * 10);
        if (verbose > 0) fprintf(stdout, "Initial Output Length: %d\n", rval);
        strcpy(strout, "");
        strncat(strout, output, rval);
        if (verbose > 0) fprintf(stdout, "Initial Output String: %s\n", strout);

        if (setup_flag != 0) {
            button_command(":SYST:PRES\r\n");
            sprintf(command, ":SENS1:FREQ:STAR %s\r\n", freq_start);
            button_command(command);
            sprintf(command, ":SENS1:FREQ:STOP %s\r\n", freq_stop);
            button_command(command);
            sprintf(command, ":SENS1:SWE:POIN %s\r\n", freq_steps);
            button_command(command);
            button_command(":CALC1:PAR:COUN 2\r\n");
            button_command(":CALC1:PAR1:SEL\r\n");
            button_command(":CALC1:PAR1:DEF S12\r\n");
            button_command(":CALC1:FORM UPH\r\n");
            button_command(":CALC1:PAR2:SEL\r\n");
            button_command(":CALC1:PAR2:DEF S12\r\n");
            button_command(":CALC1:FORM MLOG\r\n");
            button_command(":SENS1:AVER OFF\r\n");
            button_command(":SENS1:AVER:COUN 4\r\n");
            button_command(":SENS1:AVER:CLE\r\n");
            button_command(":INIT1:CONT OFF\r\n");

            printf("\n\n\7\7Calibrate Network Analyzer for S12,S21\n");
            mypause();
            button_command(":SENS1:CORR:COLL:METH:THRU 1,2\r\n");
            sleep(1);
            button_command(":SENS1:CORR:COLL:THRU 1,2\r\n");
            printf("  Doing S1,2 Calibration..wait 4 seconds\n");
            sleep(4);

            button_command(":SENS1:CORR:COLL:METH:THRU 2,1\r\n");
            sleep(1);
            button_command(":SENS1:CORR:COLL:THRU 2,1\r\n");
            printf("  Doing S2,1 Calibration..wait 4 seconds\n");
            sleep(4);
            button_command(":SENS1:CORR:COLL:SAVE\r\n");
        }
        button_command(":INIT1:IMM\r\n");
        printf("\n\nCalibration Complete\nReconfigure for Phasing Card Measurements");
        mypause();
        c = -1;
        printf("\n\nEnter Radar Name: ");
        fflush(stdin);
        scanf("%s", &radar_name);
        fflush(stdout);
        fflush(stdin);
        printf("\n\nEnter Phasing Card Number: ");
        fflush(stdin);
        fflush(stdout);
        scanf("%d", &c);
        printf("\n\nEnter Serial Number: ");
        fflush(stdin);
        fflush(stdout);
        scanf("%s", &serial_number);
        printf("Radar: <%s>  Card: %d Serial: %s\n", radar_name, c, serial_number);
        fflush(stdout);
    }
    while ((c < CARDS) && (c >= 0)) {
        if (test_flag == -1000) {
            sprintf(attenfilename, "%s%s_%s_%02d_%s%s", dir, file_prefix, radar_name, c, serial_number, atten_ext);
            if (verbose > 0) fprintf(stdout, "Using file: %s\n", attenfilename);
            fflush(stdout);
            gettimeofday(&t0, NULL);
            attenfile = fopen(attenfilename, "w");
            count = ATTENCODES;
            fwrite(&count, sizeof(int32), 1, attenfile);
            count = CARDS;
            fwrite(&count, sizeof(int32), 1, attenfile);
            count = fnum;
            fwrite(&count, sizeof(int32), 1, attenfile);
            count = 0;
            fwrite(freq, sizeof(double), fnum, attenfile);
            if (verbose > 0) {
                fprintf(stdout, "Writing beamcodes to phasing card\n");
                gettimeofday(&t2, NULL);
            }
            usleep(10000);
        } //test flag if
        if (stupid_flag) {
            while (1) {  //JDS
                if (b == 0) b = 8; //JDS
                else b = 0;//JDS
                data = b;
                beamcode = b;
                if (NEW_PMAT) {
                    printf("B: %d data: %d BC: %d\n", b, data, beamcode);
                    temp = write_data_new(IOBASE, c, beamcode, data, radar, 0);
                    sleep(2); //JDS
                    temp = write_attenuators(IOBASE, c, beamcode, 0, radar);
                } else {
                    temp = write_data_old(IOBASE, c, beamcode, b, radar);
                }
            }
        }
        printf("Programming all zeros attenuation coding\n");
        for (b = 0; b < ATTENCODES; b++) {
            data = 0;
            beamcode = b;
            if (NEW_PMAT) {
                //printf("B: %d data: %d BC: %d\n",b,data,beamcode);
                temp = write_data_new(IOBASE, c, beamcode, data, radar, 0);
                temp = write_attenuators(IOBASE, c, beamcode, data, radar);
            } else {
                temp = write_data_old(IOBASE, c, beamcode, data, radar);
            }
        }

        printf("Verifying all zero programming attenuation coding\n");
        for (b = 0; b < ATTENCODES; b++) {
            select_card(IOBASE, c, radar);
            beam_code(IOBASE, b, radar);
            usleep(10000);
            if (NEW_PMAT) temp = verify_attenuators(IOBASE, c, b, 0, radar);
            else temp = verify_data_old(IOBASE, c, b, 0, radar, 0);
        }

        printf("Programming 1-to-1 attenuation coding no phase\n");
        for (b = 0; b < ATTENCODES; b++) {
            data = b;
            beamcode = b;
            if (NEW_PMAT) {
                //printf("B: %d data: %d BC: %d\n",b,data,beamcode);
                temp = write_data_new(IOBASE, c, beamcode, 0, radar, 0);
                temp = write_attenuators(IOBASE, c, beamcode, b, radar);
            } else {
                temp = write_data_old(IOBASE, c, beamcode, 0, radar);
            }
        }
        printf("Verifying 1-to-1 programming attenuation coding\n");
        for (b = 0; b < ATTENCODES; b++) {
            select_card(IOBASE, c, radar);
            beam_code(IOBASE, b, radar);
            usleep(10000);
            if (NEW_PMAT) temp = verify_attenuators(IOBASE, c, b, b, radar);
            else temp = verify_data_old(IOBASE, c, b, b, radar, 0);
        }


        if (test_flag == -1) {
            exit(0);
        }

        sleep(10);


        if (verbose > 0) fprintf(stdout, "Measuring attens\n");
        last_collect = 0;
        current_collect = 0;
        for (b = 0; b < ATTENCODES; b++) {
            beamcode = b;
            temp = select_card(IOBASE, c, radar);
            beam_code(IOBASE, beamcode, radar);
            button_command(":INIT1:IMM\r\n");
            if (b == 0) sleep(1);
#ifdef __QNX__
            delay(wait_delay); //NO AVERAGE
#else
            usleep(wait_delay);
#endif
            take_data = 1;
            attempt = 0;
            while ((take_data) && (attempt < max_attempts)) {

                attempt++;
                button_command(":CALC1:PAR1:SEL\r\n");
                mlog_data_command(":CALC1:DATA:FDAT?\r\n", atten_phase, b);
                button_command(":CALC1:PAR2:SEL\r\n");
                mlog_data_command(":CALC1:DATA:FDAT?\r\n", atten_pwr_mag, b);
                pd_new = atten_pwr_mag[fnum - 1][b] - atten_pwr_mag[0][b];
                if (b != 0) {
                    pd_old = atten_pwr_mag[fnum - 1][last_collect] - atten_pwr_mag[0][last_collect];
                    pwr_diff = fabs(pd_new - pd_old);
                    if (pwr_diff > 1.0E-6) {
                        take_data = 0;
                    } else {  //pwr_diff too small
                        wait_delay = wait_delay + 10;
                        printf("Data Collection Error: %d. Increasing delay to %d (ms)\n", b, wait_delay);
                        printf("  Data Diff: %lf : %lf %lf\n", pwr_diff, pd_old, pd_new);
                        usleep(wait_delay);
                    }
                } else {  //b==0
                    take_data = 0;
                }
            }
            if (attempt == max_attempts) {
                printf("FATAL ERROR: Attencode %d took Max attempts %d last delay %d (ms)\n", b, attempt, wait_delay);
                exit(0);
            }
            printf("Attencode %d: Freq Index: %d Pwr: %lf Pd: %lf Wait Delay: %d\n", b, fnum - 1,
                   atten_pwr_mag[fnum - 1][b], pd_new, wait_delay);
        } // end of attencode loop
        for (i = 0; i < fnum; i++) {
            if (verbose > 1)
                printf("Freq %lf:  Pwr 0:%lf Pwr %d: %lf\n", freq[i], atten_pwr_mag[i][0], (int) ATTENCODES - 1,
                       atten_pwr_mag[i][ATTENCODES - 1]);
            fwrite(&i, sizeof(int32), 1, attenfile);
            count = fwrite(atten_phase[i], sizeof(double), ATTENCODES, attenfile);
            count = fwrite(atten_pwr_mag[i], sizeof(double), ATTENCODES, attenfile);
        }
        for (i = 0; i < fnum; i++) {
            //if (verbose > 1) printf("Freq %lf:  Phase 0:%lf Phase Max: %lf\n",freq[i],atten_phase[i][0],atten_phase[i][ATTENCODES-1]);
            if (verbose > 1)
                printf("Freq %lf:  Pwr 0:%lf Pwr Max: %lf\n", freq[i], atten_pwr_mag[i][0],
                       atten_pwr_mag[i][ATTENCODES - 1]);
            fwrite(&i, sizeof(int32), 1, attenfile);
            count = fwrite(atten_phase[i], sizeof(double), ATTENCODES, attenfile);
            count = fwrite(atten_pwr_mag[i], sizeof(double), ATTENCODES, attenfile);
            //printf("Freq index: %d Count: %d\n",i,count);
        }
        printf("Closing File\n");
        fclose(attenfile);
        c = -1;
//    if (verbose>0) fprintf(stdout,"Asking for another card:\n");
//    printf("Enter Next Card Number (CTRL-C to exit): ");
//    scanf("%d", &c);
    } // end of Card loop
}

