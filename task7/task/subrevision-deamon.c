#define _SVID_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>
#include <ftw.h>
#include <stdint.h>
#include <time.h>

//Begin: Операции над строками путей
//
const char infoName[] = "./info.txt";
const char diffName[] = "./changes.diff";
const char logName[] = "./log.log";
const char emptyName[] = "./empty.txt";
const char serviceDirectory[] = "/tmp/subrevision_deamon";

char* trackedDirectory;

size_t strchopN(char *str, size_t n) {
        size_t len = strlen(str);
        if (n > len) {
                n = len;
        }
        memmove(str, str+n, len - n + 1);
        len = len - n;
        return len;
}

char* getRelativePath(const char* path, const char* root) {
        size_t len;
        char* rPath = (char*)calloc(strlen(path), sizeof(char));
        rPath = strcpy(rPath, path);
        strchopN(rPath, strlen(root));
        len = strlen(rPath);
        if (rPath[0] == '/') {
                memmove(rPath+1, rPath, len+1);
                memmove(rPath, ".", 1);
        } else {
                memmove(rPath+2, rPath, len+1);
                memmove(rPath, "./", 2);
        }
        return rPath;
}


char* getAbsolutePath(const char* path, const char* root) {
        size_t lenPath = strlen(path);
        size_t lenRoot = strlen(root);
        char* aPath = (char*)calloc(lenPath + lenRoot, sizeof(char));
        memmove(aPath, root, lenRoot);
        memmove(aPath + lenRoot, path + 1, lenPath);
        return aPath;
}

char* removeEndBackslash(const char* path) {
        size_t length = strlen(path);
        char* str = (char*)calloc(length, sizeof(char));
        memmove(str, path, length);
        if (path[length-1] == '/') {
                memmove(str+length-1, "\0", 1);
                return str;
        }
        return str;
}

char* resolveName(const char* path) {
        char* cutpath = removeEndBackslash(path);
        size_t trace, lenName;
        if (strrchr(cutpath, '/') == NULL) {
                free(cutpath);
                return "./";
        }
        trace = (size_t)(strrchr(cutpath, '/') - cutpath);
        lenName = strlen(cutpath) - trace;
        char* str = (char*)calloc(lenName+1, sizeof(char));
        memmove(str, cutpath + trace + 1, lenName);
        memmove(str + lenName - 1, "\0", 1);
        memmove(str+2, str, lenName);
        memmove(str, "./", 2);
        free(cutpath);
        return str;
}

char* alingPath(const char* newRoot, const char* oldRoot, const char* path){
        char* relPath = getRelativePath(path, oldRoot);
        char* newPath = getAbsolutePath(relPath, newRoot);
        free(relPath);
        return newPath;
}

char* joinNameAndPath(const char* name, const char* path) {
        size_t nameLen = strlen(name);
        size_t pathLen = strlen(path);
        char* fullPath = calloc(nameLen+pathLen+1, sizeof(char));
        memmove(fullPath, path, pathLen);
        memmove(fullPath+pathLen, "/", 1);
        memmove(fullPath+pathLen+1, name, nameLen);
        return fullPath;
}

//
//End: Операции над строками путей



//Begin: структура наблюдения и управление ею
//
typedef struct observation {
	int fdInotify;
	int* fdWs;
	char** dirs;
	int capacity;
	int amount;
} obs_t;

obs_t* initializeObservation(obs_t* obs, int num) {
	free(obs->fdWs);
	obs->fdInotify = inotify_init1(IN_NONBLOCK);
	if (obs->fdInotify == -1) {
                perror("inotify_init1");
                exit(EXIT_FAILURE);
	}
	obs->fdWs = calloc(num, sizeof(int));
	if (obs->fdWs == NULL) { 
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	obs->dirs = calloc(num, sizeof(char*));
	if (obs->dirs == NULL) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	obs->capacity = num;
	obs->amount = 0;
	return obs;	
}

char* getDirectoryFromDescriptor(obs_t* obs, int wd) {
	int i = 0;
	for (i = 0; i < obs->amount; i++) {
		if (obs->fdWs[i] == wd) {
			return obs->dirs[i];
		}
	}
	return NULL;
}

int getDescriptorFromDirectory(obs_t* obs, const char* dpath) {
	int i = 0;
	for (i = 0; i < obs->amount; i++) {
		if (strcmp(dpath, obs->dirs[i]) == 0) {
			return obs->fdWs[i];
		}
	}
	return 0;
}

obs_t* registerWatcher(obs_t* obs, const char* path, uint32_t trackedEvents) {
	size_t len = strlen(path);
	(obs->amount)++;
	if (obs->amount == obs->capacity) {
		obs->capacity *= 2;
		obs->fdWs = realloc(obs->fdWs, (obs->capacity)*sizeof(int));
		obs->dirs = realloc(obs->dirs, (obs->capacity)*sizeof(char*));
	}
	obs->fdWs[obs->amount] = inotify_add_watch(obs->fdInotify, path, trackedEvents);
        if (obs->fdWs[obs->amount] == -1) {
                exit(EXIT_FAILURE);
        }
	obs->dirs[obs->amount] = calloc(len, sizeof(char));
	memmove(obs->dirs[obs->amount], path, len);
	return obs;
}

obs_t* removeWatcher(obs_t* obs, int wd) {
	int i = 0;
	struct stat st1, st2;
	if (fstat(wd, &st1) < 0) {
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < obs->amount; i++) {
		if (fstat(obs->fdWs[i], &st2) < 0) {
			exit(EXIT_FAILURE);
		} 	
		if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
			close(obs->fdWs[i]);
			if (inotify_rm_watch(obs->fdInotify, wd) < 0) {
				exit(EXIT_FAILURE);
			}
			free(obs->dirs[i]);
			(obs->amount)--;
			while(i < obs->amount) {
				obs->fdWs[i] = obs->fdWs[i+1];
				obs->dirs[i] = obs->dirs[i+1];
				i++;
			}
			return obs;	
		}
	}
	return NULL;
}
//
//End: структура наблюдения и управление ею


//Begin: Сохранение изменений отдельного файла в лог
//
void diffFiles(const char* pathArc, const char* pathRec) {
        pid_t cpid;
        switch(cpid = fork()) {
                case -1:
                        exit(EXIT_FAILURE);
                case 0:
                        execl("/usr/bin/diff", "diff", pathArc, pathRec, NULL);
                        exit(EXIT_FAILURE);
        }
        waitpid(cpid, NULL, 0);
}

void logDiff(const char* pathLog, const char* pathDif) {
        int fdLog = open(pathLog, O_WRONLY|O_APPEND);
        int fdDif = open(pathDif, O_RDONLY);
        char byte;
        while(read(fdDif, &byte, 1)) {
                write(fdLog, &byte, 1);
        }
        close(fdLog);
        close(fdDif);
}

int redirectOutput(char* path) {
        int out = open(path, O_CREAT|O_RDWR|O_TRUNC, 0666);
        if (-1 == out) {
                perror("opening cout.log");
                exit(EXIT_FAILURE);
         }
        int save_out = dup(fileno(stdout));
        if (-1 == dup2(out, fileno(stdout))) {
                perror("cannot redirect stdout");
                exit(EXIT_FAILURE);
        }
        close(out);
        return save_out;
}

void reestablishOutput(int save_out) {
        fflush(stdout);
        dup2(save_out, fileno(stdout));
        puts("back to normal output");
}

//на обработке перезаписи файла (при создании и удалении использовать empty.txt в качестве файла для сравнения)
void diffAndLog(const char* originPath, const char* updatedPath) {
	char* logPath = getAbsolutePath(logName, serviceDirectory);
	char* diffPath = getAbsolutePath(diffName, serviceDirectory);
        int save_out = redirectOutput(diffPath);
        diffFiles(originPath, updatedPath);
        reestablishOutput(save_out);
        logDiff(logPath, diffPath);
	free(diffPath);
	free(logPath);
}
//
//End: Сохранение изменений отдельного файла в лог



//Begin: Подтягивание новой версии элементов
//

void mvStoredDirectory(const char* dir, const char* destination) {
	pid_t cpid;
	switch(cpid = fork()) {
		case -1:
			exit(EXIT_FAILURE);
		case 0:
			execl("/bin/mv", "mv", dir, destination, NULL);
			exit(EXIT_FAILURE);
	}
	waitpid(cpid, NULL, 0);
	return;
}

void saveUpdatedFile(const char* source, char* destination) {
        pid_t cpid;
        switch(cpid = fork()) {
                case -1:
                        exit(EXIT_FAILURE);
                case 0:
                        execl("/bin/cp", "cp", source, destination, NULL);
                        exit(EXIT_FAILURE);
        }
        waitpid(cpid, NULL, 0);
        return;
}


//
//End: Подтягивание новой версии элементов



//Begin: Вспомогательные функции обработчиков
//

char* getArcRoot() {
	char* name = resolveName(trackedDirectory);
	char* arcRoot = getAbsolutePath(name, serviceDirectory);
	free(name);
	return arcRoot;
}

char* getEmptyFilePath() {
	char* path = getAbsolutePath(emptyName, serviceDirectory);
	int fdEmpty = open(path, O_CREAT|O_TRUNC|O_RDWR, 0666);
	close(fdEmpty);
	return path;
}

void writeTime(int fdLog) {
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf (fdLog, "local time: %s\n", asctime (timeinfo) );
}


int getFdLog() {
	char* logPath = getAbsolutePath(logName, serviceDirectory);
	int fdLog = open(logPath, O_CREAT|O_WRONLY|O_APPEND, 0666);
	free(logPath);
	return fdLog;
}

off_t fSize(const char* pathname) {
	struct stat st;
	if (stat(pathname, &st) == 0) {
		return st.st_size;
	}
	return -1;
}
//
//End: Вспомогательные функции обработчиков



//Begin: обработчики событий с файлами
//

//название события выводится в handle_create
void handle_create_file_log (const char* fpath, int fdLog) {
	char* emptyfPath = getEmptyFilePath();
	writeTime(fdLog);
	diffAndLog(emptyfPath, fpath);
	if (access(fpath, F_OK) == -1) {
                dprintf(fdLog, "During handling file was removed, so data can be corrupted\n");
        }	
	free(emptyfPath);
}

//название события выводится в handle_delete
void handle_delete_file_log (const char* fpath, int fdLog) {
        char* emptyfPath = getEmptyFilePath();
        writeTime(fdLog);
        diffAndLog(fpath, emptyfPath);
	free(emptyfPath);
}

void handle_closeWR_file_log (const char* fpath, int fdLog) {
	char* logPath = getAbsolutePath(logName, serviceDirectory);
        char* diffPath = getAbsolutePath(diffName, serviceDirectory);
	char* arcRoot = getArcRoot();
        char* pathInArc = alingPath(arcRoot, trackedDirectory, fpath);

        int save_out = redirectOutput(diffPath);
        diffFiles(pathInArc, fpath);
        reestablishOutput(save_out);
	if (fSize(diffPath)) {
		dprintf(fdLog, "Event: %s [regular file] was modified\n", fpath);
		writeTime(fdLog);
        	logDiff(logPath, diffPath);
		if (access(fpath, F_OK) == -1) {
			dprintf(fdLog, "During handling file was removed, so data can be corrupted\n");
		}
	}

	free(pathInArc);
	free(arcRoot);
        free(diffPath);
        free(logPath);
}

void handle_move_file_log (const char* fpathFrom, const char* fpathTo, int fdLog) {
	char* oldName;
	char* newName;
	if (fpathFrom == NULL) {
		dprintf(fdLog, "Event: %s [regular file] was moved from external directory\n", fpathTo);
		handle_create_file_log(fpathTo, fdLog);
		return;
	}
	if (fpathTo == NULL) {
		dprintf(fdLog, "Event: %s [regular file] was moved to external directory\n", fpathFrom);
		handle_delete_file_log(fpathFrom, fdLog);
		return;
	}
	oldName = resolveName(fpathFrom);
	newName = resolveName(fpathTo);
	if (strcmp(oldName, newName) == 0) {
		dprintf(fdLog, "Event: %s [regular file] was moved and now is %s", fpathFrom, fpathTo);
	} else {
		dprintf(fdLog, "Event: %s [regular file] was renamed and now is %s", fpathFrom, fpathTo);
	}
	writeTime(fdLog);
}

void handle_create_file_pull (const char* fpath) {
	char* arcRoot = getArcRoot(); 
	char* pathInArc = alingPath(arcRoot, trackedDirectory, fpath);
	if (access(fpath, F_OK) != -1)	{
		saveUpdatedFile(fpath, pathInArc);
	}
	free(arcRoot);
	free(pathInArc);
}

void handle_delete_file_pull (const char* path) {
	char* arcRoot = getArcRoot();
	char* pathInArc = alingPath(arcRoot, trackedDirectory, path);
	if (access(pathInArc, F_OK) != -1) {
		remove(pathInArc);
	}
	free(arcRoot);
	free(pathInArc);
}

void handle_closeWR_file_pull (const char* fpath) {
	handle_create_file_pull(fpath);
}

void handle_move_file_pull (const char* fpathFrom, const char* fpathTo, int fdLog) {
	if (fpathFrom == NULL) {
		handle_create_file_pull(fpathTo);
		return;
	}
	if (fpathTo == NULL) {
		handle_delete_file_pull(fpathFrom);
		return;
	}
	handle_create_file_pull(fpathTo);
	handle_delete_file_pull(fpathFrom);	
}

//
//End: Обработчики событий с файлами



//Begin: Обход неотслеживаемой ветви
//
const int32_t trackedEvents = IN_CLOSE_WRITE|IN_MOVE|IN_CREATE|IN_DELETE;

void handle_create_dir(obs_t* obs, char* dpath, int fdLog);
void handle_delete_dir(obs_t* obs, char* dpath, int fdLog);

//перед запуском волны в данные функции вносятся указатель на inotify объект и файловый дескриптор лога
static int walkOnRemove(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{
        static obs_t* obs;
        static int fdLog;
        if (fpath == NULL) {
                obs = (obs_t*)(ftwbuf);
                fdLog = tflag;
                return 1;
        }

        switch(tflag) {
                case FTW_F:
			dprintf(fdLog, "Event: %s [regular file] was moved to a not tracked directory\n", fpath);
			handle_delete_file_log(fpath, fdLog);
			handle_delete_file_pull(fpath);
                case FTW_D:
                        handle_delete_dir(obs, fpath, fdLog);
                case FTW_DNR:
			dprintf(fdLog, "Warning: %s [directory] can\'t be read by the process\n", fpath);
                default: break;
        }
        return 0;
}


static int walkOnAdd(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{
        static obs_t* obs;
	static int fdLog;
        if (fpath == NULL) {
                obs = (obs_t*)(ftwbuf);
		fdLog = tflag;
                return 1;
        }

        switch(tflag) {
                case FTW_F:
			dprintf(fdLog, "Event: %s [regular file] was moved from a not tracked directory\n", fpath);
                	handle_create_file_log(fpath, fdLog);
			handle_create_file_pull(fpath);
		case FTW_D:
                	handle_create_dir(obs, fpath, fdLog);
                case FTW_DNR:
			dprintf(fdLog, "Warning: %s [directory] can\'t be read by the process\n", fpath);
		default: break;
        }
        return 0;
}

int doWalkOnAdd(obs_t* obs, char *dpath, int fdLog)
{
        int numDirOpenedSim = 1;
        int flags = FTW_PHYS;
        walkFunc(NULL, NULL, fdLog, (struct FTW*)(obs));
        if (nftw(pathDir, walkOnAdd, numDirOpenedSim, flags) == -1) {
                perror("nftw");
                exit(EXIT_FAILURE);
        }
}

int doWalkOnRemove(obs_t* obs, char *dpath, int fdLog) {
	int numDirOpenedSim = 1;
        int flags = FTW_PHYS|FTW_DEPTH;
        walkFunc(NULL, NULL, fdLog, (struct FTW*)(obs));
        if (nftw(pathDir, walkOnAdd, numDirOpenedSim, flags) == -1) {
                perror("nftw");
                exit(EXIT_FAILURE);
        }
}
//
//End: Обход неотслеживаемой ветви


//Begin: Обработчики событий с директориями
//

void handle_create_dir(obs_t* obs, char* dpath, int fdLog) {
	registerWatcher(obs, dpath, trackedEvents);
	writeTime(fdLog);
	handle_create_file_pull(dpath);
}

void handle_delete_dir(obs_t* obs, char* dpath, int fdLog) {
	int wd = getDescriptorFromDirectory(obs, dpath);
	removeWatcher(obs, wd);
	writeTime(fdLog);
	handle_delete_file_pull(dpath);
}

void handle_move_dir(obs_t* obs, char* dpathFrom, char* dpathTo, int fdLog) {
	char* oldName;
	char* newName;
	if (dpathFrom == NULL) {
		dprintf(fdLog, "Event: %s [directory] was moved from external directory\n", dpathTo);
		doWalkOnAdd(obs, dpathTo, fdLog);
		return;
	}
	if (dpathTo == NULL) {
		dprintf(fdLog, "Event: %s [directory] was moved to external directory\n", dpathFrom);
		doWalkOnRemove(obs, dpathFrom, fdLog);
		return;
	}
	oldName = resolveName(dpathFrom);
        newName = resolveName(dpathTo);
        if (strcmp(oldName, newName) == 0) {
                dprintf(fdLog, "Event: %s [directory] was moved and now is %s", dpathFrom, dpathTo);
        } else {
                dprintf(fdLog, "Event: %s [directory] was renamed and now is %s", dpathFrom, dpathTo);
        }
        writeTime(fdLog);
}

//
//End: Обработчики событий с директориями



//Begin: Управление служебной директорией
//

const uint32_t trackedCorruptions = IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF;

void saveTrackedDirectory(char* source) {
        pid_t cpid;
        switch(cpid = fork()) {
                case -1:
                        exit(EXIT_FAILURE);
                case 0:
                        execl("/bin/cp", "cp", "-r", source, serviceDirectory, NULL);
                        exit(EXIT_FAILURE);
        }
        waitpid(cpid, NULL, 0);
        return;
}

int repairFile(char* path) {
        int fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if (fd < 0) {
		exit(EXIT_FAILURE);
	}	
	return fd;
}

void handle_info(obs_t* obs, const struct inotify_event* event) {
	char* infoPath = getAbsolutePath(infoName, serviceDirectory);
	int fdInfo;
	if (event) {
		removeWatcher(obs, event->wd);
	}
	fdInfo = repairFile(infoPath);
	dprintf(fdInfo, "deamon's PID: %d\n", getpid());
	close(fdInfo);
	registerWatcher(obs, infoPath, trackedCorruptions|IN_ONESHOT);
	free(infoPath);	
}

void handle_service(obs_t* obs) {
	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	int i;
	ssize_t len;
	char *ptr;
	
        while(1) {
                if ((len = read(obs->fdInotify, buf, sizeof(buf))) == -1) {
                        if (errno != EAGAIN) {
                                perror("read");
                                exit(EXIT_FAILURE);
                        }
                }

                if (len <= 0) break;

                for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                        event = (const struct inotify_event *)ptr;
			if (event->mask & trackedCorruptions) {
				handle_info(obs, event);
                        }
			printf("[service file]\n");
                }
        }

}

void createServiceDirectory(obs_t* serviceObs) {
        if (mkdir(serviceDirectory, 0777) < 0) {
                exit(EXIT_FAILURE);
        }
        char* aPath;
       	aPath = getAbsolutePath(logName, serviceDirectory);
        close(repairFile(aPath));
        free(aPath);
        handle_info(serviceObs, NULL);
	saveTrackedDirectory(trackedDirectory);
}

//
//End: Управление служебной директорией


//Begin: Распределение по обработчикам
//
void handle_moved(obs_t* obs, const struct inotify_event* event) {
	static struct inotify_event lastEvent = NULL;
	int fdLog = getFdLog();
	char* pathFrom;
	char* pathTo;
	char* fullPathFrom;
	char* fullPathTo;
	if (lastEvent == NULL) {
		if (event == NULL) {
			return;
		}
		if (event->mask & IN_MOVED) {
			lastEvent = event;
		}
		return;
	}
	if (lastEvent->mask & IN_ISDIR) {
		if (lastEvent->mask & IN_MOVED_FROM) {
			pathFrom = getDirectoryFromDescriptor(lastEvent->wd);
                        fullPathFrom = joinNameAndPath(lastEvent->name, pathFrom);
			if ((event == NULL) || !(event->mask & IN_ISDIR) || !(event->mask & IN_MOVED_TO) || !(event->cookie == lastEvent->cookie)) {
				handle_move_dir(obs, fullPathFrom, NULL, fdLog);
			} else {
				pathTo = getDirectoryFromDescriptor(event->wd);
				fullPathTo = joinNameAndPath(event->name, pathTo);
				handle_move_dir(obs, fullPathFrom, fullPathTo, fdLog);
				free(fullPathTo);
				free(pathTo);
			}
			free(fullPathFrom);
                        free(pathFrom);
			close(fdLog);
			return;
		}
		if (lastEvent->mask & IN_MOVED_TO) {
			pathTo = getDirectoryFromDescriptor(lastEvent->wd);
                        fullPathTo = joinNameAndPath(lastEvent->name, pathTo);
                        if ((event == NULL) || !(event->mask & IN_ISDIR) || !(event->mask & IN_MOVED_FROM) || !(event->cookie == lastEvent->cookie)) {
                                handle_move_dir(obs, NULL, fullPathTo, fdLog);
                        } else {
                                pathFrom = getDirectoryFromDescriptor(event->wd);
                                fullPathFrom = joinNameAndPath(event->name, pathFrom);
                                handle_move_dir(obs, fullPathFrom, fullPathTo, fdLog);
                                free(fullPathFrom);
                                free(pathFrom);
                        }
                        free(fullPathTo);
                        free(pathTo);
                        close(fdLog);
                        return;
		}
	} else {
                if (lastEvent->mask & IN_MOVED_FROM) {
                        pathFrom = getDirectoryFromDescriptor(lastEvent->wd);
                        fullPathFrom = joinNameAndPath(lastEvent->name, pathFrom);
                        if ((event == NULL) || (event->mask & IN_ISDIR) || !(event->mask & IN_MOVED_TO) || !(event->cookie == lastEvent->cookie)) {
                 	        handle_move_file_log(fullPathFrom, NULL, fdLog);
                                handle_move_file_pull(fullPathFrom, NULL);

			} else {
                                pathTo = getDirectoryFromDescriptor(event->wd);
                                fullPathTo = joinNameAndPath(event->name, pathTo);
                                handle_move_file_log(fullPathFrom, fullPathTo, fdLog);
				handle_move_file_pull(fullPathFrom, fullPathTo);
                                free(fullPathTo);
                                free(pathTo);
                        }
                        free(fullPathFrom);
                        free(pathFrom);
                        close(fdLog);
                        return;
                }
                if (lastEvent->mask & IN_MOVED_TO) {
                        pathTo = getDirectoryFromDescriptor(lastEvent->wd);
                        fullPathTo = joinNameAndPath(lastEvent->name, pathTo);
                        if ((event == NULL) || (event->mask & IN_ISDIR) || !(event->mask & IN_MOVED_FROM) || !(event->cookie == lastEvent->cookie)) {
                                handle_move_file_log(NULL, fullPathTo, fdLog);
				handle_move_file_pull(NULL, fullPathTo);
                        } else {
                                pathFrom = getDirectoryFromDescriptor(event->wd);
                                fullPathFrom = joinNameAndPath(event->name, pathFrom);
                                handle_move_file_log(fullPathFrom, fullPathTo, fdLog);
				handle_move_file_pull(fullPathFrom, fullPathTo);
                                free(fullPathFrom);
                                free(pathFrom);
                        }
                        free(fullPathTo);
                        free(pathTo);
                        close(fdLog);
                        return;
		
		
	}
}

void handle_create(obs_t* obs, const struct inotify_event* event) {
	int fdLog = getFdLog();

        dprintf(fdLog, "Event: %s [regular file] was created\n", event->name);
}
//(event->name) NEED FIX!!!!!!!!!!
void handle_delete(obs_t* obs, const struct inotify_event* event) {
	int fdLog = getFdLog();
        dprintf(fdLog, "Event: %s [regular file] was deleted\n", event->name);
}

void handle_modify(obs_t* obs, const struct inotify_event* event) {
	
}
//
//End: Распределение по обработчикам

void handle_events(int fdInotify, int *wd) {
	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	int i;
	ssize_t len;
	char *ptr;

	while(1) {
		if ((len = read(fdInotify, buf, sizeof(buf))) == -1) {
			if (errno != EAGAIN) {
				perror("read");
				exit(EXIT_FAILURE);
			}
		}

		if (len <= 0) break;

		for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
			event = (const struct inotify_event *)ptr;
			if (event->mask & IN_MOVED_FROM) {
				printf("IN_MOVED_FROM: ");
			}
			if (event->mask & IN_MOVED_TO) {
				printf("IN_MOVED_TO: "); 
			}
			if (event->mask & IN_CLOSE_WRITE) {
				printf("IN_CLOSE_WRITE: ");
			}
			if (event->mask & IN_DELETE) {
				printf("IN_DELETE: ");
			}	
			if (event->mask & IN_CREATE) {
				printf("IN_CREATE: ");
			}
			
			if (event->len) {
				printf("%s", event->name);
			}
			
	                if (event->len)
		                printf("%s", event->name);

			if (event->mask & IN_ISDIR) {
				printf(" [directory]\n");
			} else {
				printf(" [file]\n");
			}
		}	
	}
}

void deamon(char* pathTrackedDir) {
	int fdTrackedDir = open(pathTrackedDir, O_DIRECTORY);
	int fdArchivedDir;
	int fdInotify = inotify_init1(IN_NONBLOCK);
	int fdInotifyService = inotify_init1(IN_NONBLOCK);
}

int main(int argc, char* argv[]) {
        char buf;
        int fd, i, poll_num;
        int *wd;
        nfds_t nfds;
        struct pollfd fds[2];
	if (argc < 2) {
        	printf("Usage: %s PATH [PATH ...]\n", argv[0]);
        	exit(EXIT_FAILURE);
        }

        printf("Press ENTER key to terminate.\n");

           /* Create the file descriptor for accessing the inotify API */

        fd = inotify_init1(IN_NONBLOCK);
        if (fd == -1) {
        	perror("inotify_init1");
                exit(EXIT_FAILURE);
        }

           /* Allocate memory for watch descriptors */

        wd = calloc(argc, sizeof(int));
        if (wd == NULL) {
                perror("calloc");
                exit(EXIT_FAILURE);
        }

        for (i = 1; i < argc; i++) {
                wd[i] = inotify_add_watch(fd, argv[i], trackedEvents);
                if (wd[i] == -1) {
                   	perror("inotify_add_watch");
                   	exit(EXIT_FAILURE);
               	}
        }
	nfds = 2;

        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;

        fds[1].fd = fd;
        fds[1].events = POLLIN;

        printf("Listening for events.\n");
        while (1) {
        	poll_num = poll(fds, nfds, -1);
                if (poll_num == -1) {
                	if (errno == EINTR) continue;
		        perror("poll");
                  	exit(EXIT_FAILURE);
               	}
               	if (poll_num > 0) {
                	if (fds[0].revents & POLLIN) {
                       		while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n') continue;
                       		break;
               		}
                   	if (fds[1].revents & POLLIN) {
                       	handle_events(fd, wd);
                   	}
               }
	}

        printf("Listening for events stopped.\n");

        close(fd);

        free(wd);
        exit(EXIT_SUCCESS);
}
	
