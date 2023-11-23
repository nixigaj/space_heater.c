/*
 * 'space_heater.c'
 * https://github.com/nixigaj/space_heater.c
 *
 * A simple program to heat up your room with your computer during the winter by
 * stressing all CPU cores on the system. I genuinely use this sometimes. It is
 * written in ANSI C, and can be compiled on both Windows and POSIX-compatible
 * systems.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 Erik Junsved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
/* Windows imlementation */

#include <strsafe.h>
#include <windows.h>

static HANDLE stdOut;
static HANDLE stdErr;

static volatile BOOL stop = FALSE;

DWORD WINAPI CpuHeater(LPVOID arg);
BOOL WINAPI ExitHandler(DWORD signal);

INT main(void) {
	SYSTEM_INFO sysinfo;
	INT numThreads;
	HANDLE* threads;
	CHAR* message;
	CHAR messageBuffer[128];
	INT i;
	HRESULT res;

	stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdOut == NULL || stdOut == INVALID_HANDLE_VALUE) {
		OutputDebugStringA("ERROR: Failed to initialize standard output");
		ExitProcess(EXIT_FAILURE);
	}

	stdErr = GetStdHandle(STD_ERROR_HANDLE);
	if (stdErr == NULL || stdErr == INVALID_HANDLE_VALUE) {
		OutputDebugStringA("ERROR: Failed to initialize standard error ouput");
		ExitProcess(EXIT_FAILURE);
	}

	if (SetConsoleCtrlHandler(ExitHandler, TRUE) == FALSE) {
		message = "ERROR: Failed to set console ctrl handler\n";
		WriteConsoleA(stdErr, message, (DWORD)strlen(message), NULL, NULL);
		ExitProcess(EXIT_FAILURE);
	}

	GetSystemInfo(&sysinfo);
	numThreads = sysinfo.dwNumberOfProcessors;
	if (numThreads <= 0) {
		message = "ERROR: Failed to get number of processors\n";
		WriteConsoleA(stdErr, message, (DWORD)strlen(message), NULL, NULL);
		ExitProcess(EXIT_FAILURE);
	}

	threads = (HANDLE*)HeapAlloc(GetProcessHeap(), 0,
		numThreads * sizeof(HANDLE));
	if (threads == NULL) {
		message = "ERROR: Failed to allocate memory for threads\n";
		WriteConsoleA(stdErr, message, (DWORD)strlen(message), NULL, NULL);
		ExitProcess(EXIT_FAILURE);
	}

	for (i = 0; i < numThreads; i++) {
		threads[i] = CreateThread(NULL, 0, CpuHeater, NULL, 0, NULL);
		if (threads[i] == NULL) {
			message = "ERROR: Failed to create thread %d.\n";
			res = StringCchPrintfA(messageBuffer, ARRAYSIZE(messageBuffer),
				message, i);
			if (res == S_OK) {
				WriteConsoleA(stdErr, messageBuffer, (DWORD)strlen(message),
					NULL, NULL);
			}
			else {
				message = "ERROR: Failed to create a thread\n";
				WriteConsoleA(stdErr, message, (DWORD)strlen(message), NULL,
					NULL);
			}
			HeapFree(GetProcessHeap(), 0, threads);
			ExitProcess(EXIT_FAILURE);
		}
	}

	message = "Started %d worker threads\n";
	res = StringCchPrintfA(messageBuffer, ARRAYSIZE(messageBuffer),
		message, numThreads);
	if (res == S_OK) {
		WriteConsoleA(stdOut, messageBuffer, (DWORD)strlen(messageBuffer),
			NULL, NULL);
	}
	else {
		message = "Started worker threads\n";
		WriteConsoleA(stdOut, message, (DWORD)strlen(message), NULL,
			NULL);
	}

	WaitForMultipleObjects(numThreads, threads, TRUE, INFINITE);

	for (i = 0; i < numThreads; ++i) {
		CloseHandle(threads[i]);
	}

	HeapFree(GetProcessHeap(), 0, threads);

	return EXIT_SUCCESS;
}

DWORD WINAPI CpuHeater(LPVOID arg) {
	UNREFERENCED_PARAMETER(arg);

	while (!stop) {}
	return 0;
}

BOOL WINAPI ExitHandler(CONST DWORD signal) {
	CHAR* message;
	CHAR messageBuffer[128];
	HRESULT res;

	message = "Received exit signal %lu: Exiting...\n";
	res = StringCchPrintfA(messageBuffer, ARRAYSIZE(messageBuffer),
		message, signal);
	if (res == S_OK) {
		WriteConsoleA(stdErr, messageBuffer, (DWORD)strlen(messageBuffer),
			NULL, NULL);
	}
	else {
		message = "Received exit signal: Exiting...\n";
		WriteConsoleA(stdErr, message, (DWORD)strlen(message), NULL,
			NULL);
	}

	stop = TRUE;
	return TRUE;
}

#elif defined(__linux) || defined(__unix) || defined(__posix)
/* POSIX-compatible implementation */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile int stop = 0;

void* cpu_heater(void* arg);
void exit_handler(int signal);

int main(void) {
	const int numThreads = sysconf(_SC_NPROCESSORS_ONLN);
	pthread_t* threads;
	int i;

	signal(SIGINT, exit_handler);
	signal(SIGTERM, exit_handler);

	if (numThreads <= 0) {
		fprintf(stderr, "Error getting the number of processors.\n");
		return EXIT_FAILURE;
	}

	threads = malloc(numThreads * sizeof(pthread_t));
	if (threads == NULL) {
		fprintf(stderr, "Error allocating memory for threads.\n");
		return EXIT_FAILURE;
	}

	for (i = 0; i < numThreads; i++) {
		if (pthread_create(&threads[i], NULL, cpu_heater, NULL) != 0) {
			fprintf(stderr, "Error creating thread %d.\n", i);
			free(threads);
			return EXIT_FAILURE;
		}
	}

	fprintf(stdout, "Started %d worker threads\n", numThreads);

	for (i = 0; i < numThreads; ++i) {
		pthread_join(threads[i], NULL);
	}

	free(threads);
	return EXIT_SUCCESS;
}

void* cpu_heater(void* arg) {
	(void)arg;

	while (!stop) {}
	return NULL;
}

void exit_handler(const int signal) {
	fprintf(stdout, "Received exit signal %d: Exiting...\n", signal);
	stop = 1;
}

#else
#error "Unsupported platform"
#endif
