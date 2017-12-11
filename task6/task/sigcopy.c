#include<signal.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>

#define SIGDATA SIGRTMIN
#define SIGREQUEST SIGRTMIN+1
#define SIGPERMISSION SIGRTMIN+2
#define SIGEND SIGRTMIN+3

#define ONEXIT SIGUSR1

void handlerTermination(int signal) {
	static char wasSIGCHLD = 0, wasSIGTERM = 0;
	switch(signal) {
		case SIGCHLD:
			wasSIGCHLD = 1;
			break;
		case SIGTERM:
			wasSIGTERM = 1;
			exit(EXIT_FAILURE);
		case ONEXIT:
			if (wasSIGCHLD) {
				printf("SIGCHLD has been received by PID:%d\n", getpid());
			}
			if (wasSIGTERM) {
				printf("SIGTERM has been received by PID:%d\n", getpid());
			}
	}
}

int fd_destin;
int fd_source;
pid_t cpid;

void handlerData(int signal, siginfo_t* si, void* arg) {
	char byte = (char)(si->si_value.sival_int);
	write(fd_destin, &byte, 1);
}

void handlerRequest(int signal) {
	kill(cpid, SIGPERMISSION);
}

void handlerIdle(int signal) {

}

void customizedOnExit() {
	raise(ONEXIT);
}

void preparation() {
	struct sigaction record, actData, actRequest, actPermission, actEnd;
	sigset_t recordSet, transmissionSet;

	memset(&record, 0, sizeof(record));
	memset(&actData, 0, sizeof(actData));
	memset(&actRequest, 0, sizeof(actRequest));
	memset(&actPermission, 0, sizeof(actPermission));
	memset(&actEnd, 0, sizeof(actEnd));

	sigemptyset(&recordSet);
	sigaddset(&recordSet, SIGCHLD);
	sigaddset(&recordSet, SIGTERM);
	sigaddset(&recordSet, ONEXIT);
	record.sa_mask = recordSet;

	sigemptyset(&transmissionSet);
	sigaddset(&transmissionSet, SIGDATA);
	sigaddset(&transmissionSet, SIGREQUEST);
	sigaddset(&transmissionSet, SIGPERMISSION);
	sigaddset(&transmissionSet, SIGEND);
	actEnd.sa_mask = actPermission.sa_mask = actRequest.sa_mask = actData.sa_mask = transmissionSet;

	record.sa_handler = handlerTermination;
	sigaction(SIGCHLD, &record, NULL);
	sigaction(SIGTERM, &record, NULL);
	sigaction(ONEXIT, &record, NULL);

	actData.sa_flags = SA_SIGINFO;
	actData.sa_sigaction = handlerData;
	sigaction(SIGDATA, &actData, NULL);

	actRequest.sa_handler = handlerRequest;
	sigaction(SIGREQUEST, &actRequest, NULL);

	actPermission.sa_handler = actEnd.sa_handler = handlerIdle;
	sigaction(SIGPERMISSION, &actPermission, NULL);
	sigaction(SIGEND, &actEnd, NULL);

	atexit(customizedOnExit);
	return;
}

void readAndSend() {
	int signal;
        pid_t ppid = getppid();
        sigset_t permission;
        union sigval byteWrapper;
        char buffer;
        sigemptyset(&permission);
        sigaddset(&permission, SIGPERMISSION);
	sigprocmask(SIG_BLOCK, &permission, NULL);
        while(read(fd_source, &buffer, 1)) {
                byteWrapper.sival_int = buffer;
                while (sigqueue(ppid, SIGDATA, byteWrapper) < 0) {
                        if (errno == EAGAIN) {
				kill(ppid, SIGREQUEST);
				sigwait(&permission, &signal);
                        }
                }
        }
        kill(ppid, SIGEND);
	return;
}

void blockParentUntilFinish() {
	sigset_t end;
	sigemptyset(&end);
	sigaddset(&end, SIGEND);
	sigprocmask(SIG_BLOCK, &end, NULL);
	return;
}

void getAndWrite() {
	int signal;
	sigset_t end;
	sigemptyset(&end);
	sigaddset(&end, SIGEND);
	sigwait(&end, &signal);
	return;
}

int main(int argc, char* argv[]) {
	preparation();
	fd_source = open(argv[1], O_RDONLY);
	fd_destin = creat(argv[2], 0666);
	blockParentUntilFinish();
	switch(cpid = fork()) {
		case -1:
			perror("ERROR in fork: ");
			exit(EXIT_FAILURE);
		case 0:
		        readAndSend();
			break;
		default:
			getAndWrite();
			break;
	}
	close(fd_destin);
	close(fd_source);
	exit(EXIT_SUCCESS);
} 
