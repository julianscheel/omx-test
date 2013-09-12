/**
   Based on code
   Copyright (C) 2007-2009 STMicroelectronics
   Copyright (C) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
   under the LGPL

   Based on code from http://jan.newmarch.name/LinuxSound/Sampled/OpenMAX/
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>

#include <bcm_host.h>
#include <OMX_Broadcom.h>

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))

OMX_ERRORTYPE err;
OMX_HANDLETYPE handle;
OMX_VERSIONTYPE specVersion, compVersion;

int fd = 0;
unsigned int filesize;
static OMX_BOOL bEOS=OMX_FALSE;

OMX_U32 nBufferSize;
int nBuffers;

const char *ErrorToString(OMX_ERRORTYPE error)
{
    static const char *psz_names[] = {
        "OMX_ErrorInsufficientResources", "OMX_ErrorUndefined",
        "OMX_ErrorInvalidComponentName", "OMX_ErrorComponentNotFound",
        "OMX_ErrorInvalidComponent", "OMX_ErrorBadParameter",
        "OMX_ErrorNotImplemented", "OMX_ErrorUnderflow",
        "OMX_ErrorOverflow", "OMX_ErrorHardware", "OMX_ErrorInvalidState",
        "OMX_ErrorStreamCorrupt", "OMX_ErrorPortsNotCompatible",
        "OMX_ErrorResourcesLost", "OMX_ErrorNoMore", "OMX_ErrorVersionMismatch",
        "OMX_ErrorNotReady", "OMX_ErrorTimeout", "OMX_ErrorSameState",
        "OMX_ErrorResourcesPreempted", "OMX_ErrorPortUnresponsiveDuringAllocation",
        "OMX_ErrorPortUnresponsiveDuringDeallocation",
        "OMX_ErrorPortUnresponsiveDuringStop", "OMX_ErrorIncorrectStateTransition",
        "OMX_ErrorIncorrectStateOperation", "OMX_ErrorUnsupportedSetting",
        "OMX_ErrorUnsupportedIndex", "OMX_ErrorBadPortIndex",
        "OMX_ErrorPortUnpopulated", "OMX_ErrorComponentSuspended",
        "OMX_ErrorDynamicResourcesUnavailable", "OMX_ErrorMbErrorsInFrame",
        "OMX_ErrorFormatNotDetected", "OMX_ErrorContentPipeOpenFailed",
        "OMX_ErrorContentPipeCreationFailed", "OMX_ErrorSeperateTablesUsed",
        "OMX_ErrorTunnelingUnsupported",
        "OMX_Error unknown"
    };

    if(error == OMX_ErrorNone) return "OMX_ErrorNone";

    error -= OMX_ErrorInsufficientResources;

    if((unsigned int)error > sizeof(psz_names)/sizeof(char*)-1)
        error = (OMX_STATETYPE)(sizeof(psz_names)/sizeof(char*)-1);
    return psz_names[error];
}

pthread_mutex_t mutex;
OMX_STATETYPE currentState = OMX_StateLoaded;
pthread_cond_t stateCond;

void waitFor(OMX_STATETYPE state) {
	pthread_mutex_lock(&mutex);
	while (currentState != state)
	pthread_cond_wait(&stateCond, &mutex);
	pthread_mutex_unlock(&mutex);
}

void wakeUp(OMX_STATETYPE newState) {
	pthread_mutex_lock(&mutex);
	printf("State change!");
	currentState = newState;
	pthread_cond_signal(&stateCond);
	pthread_mutex_unlock(&mutex);
}

pthread_mutex_t empty_mutex;
int emptyState = 0;
OMX_BUFFERHEADERTYPE* pEmptyBuffer;
OMX_BUFFERHEADERTYPE* pFillBuffer;
pthread_cond_t emptyStateCond;

void waitForEmpty() {
	pthread_mutex_lock(&empty_mutex);
	while (emptyState == 1)
	pthread_cond_wait(&emptyStateCond, &empty_mutex);
	emptyState = 1;
	pthread_mutex_unlock(&empty_mutex);
}

void wakeUpEmpty(OMX_BUFFERHEADERTYPE* pBuffer) {
	pthread_mutex_lock(&empty_mutex);
	emptyState = 0;
	pEmptyBuffer = pBuffer;
	pthread_cond_signal(&emptyStateCond);
	pthread_mutex_unlock(&empty_mutex);
}

void mutex_init() {
	int n = pthread_mutex_init(&mutex, NULL);
	if (n != 0)
		fprintf(stderr, "Can't init state mutex\n");
	n = pthread_mutex_init(&empty_mutex, NULL);
	if (n != 0)
		fprintf(stderr, "Can't init empty mutex\n");
}

static void display_help(char **argv) {
	fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
}


/** Gets the file descriptor's size
 * @return the size of the file. If size cannot be computed
 * (i.e. stdin, zero is returned)
 */
static int getFileSize(int fd) {

	struct stat input_file_stat;
	int err;

	/* Obtain input file length */
	err = fstat(fd, &input_file_stat);
	if(err){
		fprintf(stderr, "fstat failed",0);
		exit(-1);
	}
	return input_file_stat.st_size;
}

OMX_ERRORTYPE cEventHandler(
				OMX_HANDLETYPE hComponent,
				OMX_PTR pAppData,
				OMX_EVENTTYPE eEvent,
				OMX_U32 Data1,
				OMX_U32 Data2,
				OMX_PTR pEventData) {

	fprintf(stderr, "%s: Event %d\n", __func__, eEvent);
	if(eEvent == OMX_EventCmdComplete) {
		if(Data1 == OMX_CommandStateSet) {
			fprintf(stderr, "Component State changed in ", 0);
			switch ((int)Data2) {
			case OMX_StateInvalid:
				fprintf(stderr, "OMX_StateInvalid\n", 0);
				break;
			case OMX_StateLoaded:
				fprintf(stderr, "OMX_StateLoaded\n", 0);
				break;
			case OMX_StateIdle:
				fprintf(stderr, "OMX_StateIdle\n",0);
				break;
			case OMX_StateExecuting:
				fprintf(stderr, "OMX_StateExecuting\n",0);
				break;
			case OMX_StatePause:
				fprintf(stderr, "OMX_StatePause\n",0);
				break;
			case OMX_StateWaitForResources:
				fprintf(stderr, "OMX_StateWaitForResources\n",0);
				break;
			}
			wakeUp((int) Data2);
		} else if (Data1 == OMX_CommandPortEnable) {
				fprintf(stderr, "OMX_CommandPortEnable\n",0);
		} else if (Data1 == OMX_CommandPortDisable) {
				fprintf(stderr, "OMX_CommandPortDisable\n",0);
		}
	} else if(eEvent == OMX_EventBufferFlag) {
		if((int)Data2 == OMX_BUFFERFLAG_EOS) {
				fprintf(stderr, "OMX_BUFFERFLAG_EOS\n",0);
		}
	} else if(eEvent == OMX_EventError) {
		fprintf(stderr, "OMX_EventError: %s, %u\n",
				ErrorToString((OMX_ERRORTYPE)Data1), (unsigned int)Data2);
	} else {
		fprintf(stderr, "Param1 is %i\n", (int)Data1);
		fprintf(stderr, "Param2 is %i\n", (int)Data2);
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE cEmptyBufferDone(
				   OMX_HANDLETYPE hComponent,
				   OMX_PTR pAppData,
				   OMX_BUFFERHEADERTYPE* pBuffer) {

	fprintf(stderr, "Hi there, I am in the %s callback.\n", __func__);
	if (bEOS)
		fprintf(stderr, "Buffers emptied, exiting\n");

	wakeUpEmpty(pBuffer);
	fprintf(stderr, "Exiting callback\n");

	return OMX_ErrorNone;
}

OMX_ERRORTYPE cFillBufferDone(
				   OMX_HANDLETYPE hComponent,
				   OMX_PTR pAppData,
				   OMX_BUFFERHEADERTYPE* pBuffer) {

	fprintf(stderr, "Hi there, I am in the %s callback.\n", __func__);

	return OMX_ErrorNone;
}

OMX_CALLBACKTYPE callbacks	= { .EventHandler = cEventHandler,
				.EmptyBufferDone = cEmptyBufferDone,
				.FillBufferDone = cFillBufferDone,
};

void printState() {
	OMX_STATETYPE state;

	err = OMX_GetState(handle, &state);
	if (err != OMX_ErrorNone) {
		fprintf(stderr, "Error on getting state\n");
		exit(1);
	}

	switch (state) {
		case OMX_StateLoaded: fprintf(stderr, "StateLoaded\n"); break;
		case OMX_StateIdle: fprintf(stderr, "StateIdle\n"); break;
		case OMX_StateExecuting: fprintf(stderr, "StateExecuting\n"); break;
		case OMX_StatePause: fprintf(stderr, "StatePause\n"); break;
		case OMX_StateWaitForResources: fprintf(stderr, "StateWiat\n"); break;
		default: fprintf(stderr, "State unknown\n"); break;
	}
}


static void setHeader(OMX_PTR header, OMX_U32 size) {
	/* header->nVersion */
	OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
	/* header->nSize */
	*((OMX_U32*)header) = size;

	/* for 1.2
	   ver->s.nVersionMajor = OMX_VERSION_MAJOR;
	   ver->s.nVersionMinor = OMX_VERSION_MINOR;
	   ver->s.nRevision = OMX_VERSION_REVISION;
	   ver->s.nStep = OMX_VERSION_STEP;
	*/
	ver->s.nVersionMajor = specVersion.s.nVersionMajor;
	ver->s.nVersionMinor = specVersion.s.nVersionMinor;
	ver->s.nRevision = specVersion.s.nRevision;
	ver->s.nStep = specVersion.s.nStep;
}

/**
 * Disable unwanted ports, or we can't transition to Idle state
 */
void disablePort(OMX_INDEXTYPE paramType) {
	OMX_PORT_PARAM_TYPE param;
	int nPorts;
	int startPortNumber;
	int n;

	setHeader(&param, sizeof(OMX_PORT_PARAM_TYPE));
	err = OMX_GetParameter(handle, paramType, &param);
	if(err != OMX_ErrorNone){
	fprintf(stderr, "Error in getting OMX_PORT_PARAM_TYPE parameter\n", 0);
	exit(1);
	}
	startPortNumber = ((OMX_PORT_PARAM_TYPE)param).nStartPortNumber;
	nPorts = ((OMX_PORT_PARAM_TYPE)param).nPorts;
	if (nPorts > 0) {
		fprintf(stderr, "Other has %d ports\n", nPorts);
		/* and disable it */
		for (n = 0; n < nPorts; n++) {
			err = OMX_SendCommand(handle, OMX_CommandPortDisable, n + startPortNumber, NULL);
			if (err != OMX_ErrorNone) {
				fprintf(stderr, "Error on setting port to disabled\n");
				exit(1);
			}
		}
	}
}

/* For the RPi name can be "hdmi" or "local" */
void setOutputDevice(const char *name) {
   int32_t success = -1;
   OMX_CONFIG_BRCMAUDIODESTINATIONTYPE arDest;

   if (name && strlen(name) < sizeof(arDest.sName)) {
	   setHeader(&arDest, sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE));
	   strcpy((char *)arDest.sName, name);

	   err = OMX_SetParameter(handle, OMX_IndexConfigBrcmAudioDestination, &arDest);
	   if (err != OMX_ErrorNone) {
		   fprintf(stderr, "Error on setting audio destination\n");
		   exit(1);
	   }
   }
}

void setPCMMode(int startPortNumber) {
	OMX_AUDIO_PARAM_PCMMODETYPE sPCMMode;

	setHeader(&sPCMMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	sPCMMode.nPortIndex = startPortNumber;
	sPCMMode.nSamplingRate = 48000;
	sPCMMode.nChannels;

	err = OMX_SetParameter(handle, OMX_IndexParamAudioPcm, &sPCMMode);
	if(err != OMX_ErrorNone) {
		fprintf(stderr, "PCM mode unsupported\n");
		return;
	} else {
		fprintf(stderr, "PCM mode supported\n");
		fprintf(stderr, "PCM sampling rate %d\n", sPCMMode.nSamplingRate);
		fprintf(stderr, "PCM nChannels %d\n", sPCMMode.nChannels);
	}
}

int main(int argc, char** argv) {

	OMX_PORT_PARAM_TYPE param;
	OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
	OMX_AUDIO_PORTDEFINITIONTYPE sAudioPortDef;
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE sPCMMode;
	OMX_BUFFERHEADERTYPE **inBuffers[2];

	char *componentName = "OMX.broadcom.video_render";
	unsigned char name[OMX_MAX_STRINGNAME_SIZE];
	OMX_UUIDTYPE uid;
	int startPortNumber;
	int nPorts;
	int i, n;

	bcm_host_init();

	fprintf(stderr, "Thread id is %p\n", pthread_self());
	if(argc < 2){
		display_help(argv);
		exit(1);
	}

	fd = open(argv[1], O_RDONLY);
	if(fd < 0){
		perror("Error opening input file\n");
		exit(1);
	}
	filesize = getFileSize(fd);


	err = OMX_Init();
	if(err != OMX_ErrorNone) {
		fprintf(stderr, "OMX_Init() failed\n", 0);
		exit(1);
	}
	/** Ask the core for a handle to the audio render component
	 */
	err = OMX_GetHandle(&handle, componentName, NULL /*app private data */, &callbacks);
	if(err != OMX_ErrorNone) {
		fprintf(stderr, "OMX_GetHandle failed\n", 0);
		exit(1);
	}
	err = OMX_GetComponentVersion(handle, name, &compVersion, &specVersion, &uid);
	if(err != OMX_ErrorNone) {
		fprintf(stderr, "OMX_GetComponentVersion failed\n", 0);
		exit(1);
	}

	/** disable other ports */
	disablePort(OMX_IndexParamOtherInit);

	/** Get audio port information */
	setHeader(&param, sizeof(OMX_PORT_PARAM_TYPE));
	err = OMX_GetParameter(handle, OMX_IndexParamVideoInit, &param);
	if(err != OMX_ErrorNone){
		fprintf(stderr, "Error in getting OMX_PORT_PARAM_TYPE parameter\n", 0);
		exit(1);
	}
	startPortNumber = ((OMX_PORT_PARAM_TYPE)param).nStartPortNumber;
	nPorts = ((OMX_PORT_PARAM_TYPE)param).nPorts;
	if (nPorts > 2) {
		fprintf(stderr, "Image device has more than one port\n");
		exit(1);
	}

	for (i = 0; i < nPorts; i++) {
		/* Get and check port information */
		setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		sPortDef.nPortIndex = startPortNumber + i;
		err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);

		if(err != OMX_ErrorNone) {
			fprintf(stderr, "Error in getting OMX_PORT_DEFINITION_TYPE parameter\n", 0);
			exit(1);
		}
		if (sPortDef.eDomain != OMX_PortDomainVideo) {
			fprintf(stderr, "Port %d is not a video port\n", sPortDef.nPortIndex);
			exit(1);
		}

		if (sPortDef.eDir == OMX_DirInput)
			fprintf(stdout, "Port %d is an input port\n", sPortDef.nPortIndex);
		else
			fprintf(stdout, "Port %d is an output port\n", sPortDef.nPortIndex);

		if (sPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420PackedPlanar)
			fprintf(stderr, "Port color Format is YUV420PackedPlanar\n");
		else
			fprintf(stderr, "Port has unknown color format\n");

		/* Set Image Format -- FIXME: hardcoded */
		sPortDef.format.video.nFrameWidth = 1920;
		sPortDef.format.video.nFrameHeight = 1080;
		sPortDef.format.video.nStride =
			ALIGN(sPortDef.format.video.nFrameWidth, 32);
		sPortDef.format.image.nSliceHeight =
			ALIGN(sPortDef.format.video.nFrameHeight, 16);
		sPortDef.nBufferSize = sPortDef.format.image.nStride *
			sPortDef.format.image.nSliceHeight * 3 / 2;

		/* Create minimum number of buffers for the port */
		nBuffers = sPortDef.nBufferCountActual = sPortDef.nBufferCountMin;
		fprintf(stderr, "Number of bufers is %d\n", nBuffers);
		err = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);
		if(err != OMX_ErrorNone){
			fprintf(stderr, "Error in setting OMX_PORT_PARAM_TYPE parameter\n", 0);
			exit(1);
		}
		if (sPortDef.bEnabled) {
			fprintf(stderr, "Port %d is enabled\n", sPortDef.nPortIndex);
		} else {
			fprintf(stderr, "Port %d is not enabled\n", sPortDef.nPortIndex);
		}
	}

	/* call to put state into idle before allocating buffers */
	printf("OMX_CommandStateSet, OMX_StateIdle\n");
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		fprintf(stderr, "Error on setting state to idle\n");
		exit(1);
	}

	for (i = 0; i < nPorts; i++) {
		/* Get and check port information */
		setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		sPortDef.nPortIndex = startPortNumber + i;
		err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);

		if(err != OMX_ErrorNone) {
			fprintf(stderr, "Error in getting OMX_PORT_DEFINITION_TYPE parameter\n", 0);
			exit(1);
		}

		if (!sPortDef.bEnabled) {
			printf("OMX_CommandPortEnable, %d\n", startPortNumber);
			err = OMX_SendCommand(handle, OMX_CommandPortEnable, startPortNumber, NULL);
			if (err != OMX_ErrorNone) {
				fprintf(stderr, "Error on setting port to enabled\n");
				exit(1);
			}
		}
	}

	/* Configure buffers for the port */
	for (i = 0; i < nPorts; i++) {
		setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		sPortDef.nPortIndex = startPortNumber + i;
		err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);

		nBufferSize = sPortDef.nBufferSize;
		fprintf(stderr, "Port %d has %d buffers of size %d\n", sPortDef.nPortIndex,
				nBuffers, nBufferSize);

		inBuffers[i] = malloc(nBuffers * sizeof(OMX_BUFFERHEADERTYPE *));
		if (inBuffers[i] == NULL) {
			fprintf(stderr, "Can't allocate buffers\n");
			exit(1);
		}

		for (n = 0; n < nBuffers; n++) {
			err = OMX_AllocateBuffer(handle, inBuffers[i] + n, startPortNumber + i, NULL,
						 nBufferSize);
			if (err != OMX_ErrorNone) {
				fprintf(stderr, "Error on AllocateBuffer in 1%i\n", err);
				exit(1);
			}
		}
	}

	printf("Transition to Idle\n");
	/* Make sure we've reached Idle state */
	waitFor(OMX_StateIdle);

	printf("Transition to Executing\n");
	/* Now try to switch to Executing state */
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if(err != OMX_ErrorNone){
		exit(1);
	}

	/* One buffer is the minimum for Broadcom component, so use that */
	pEmptyBuffer = inBuffers[0][0];
	emptyState = 1;
	/* Fill and empty buffer */
	for (;;) {
		int data_read = read(fd, pEmptyBuffer->pBuffer, nBufferSize);
		pEmptyBuffer->nFilledLen = data_read;
		pEmptyBuffer->nOffset = 0;
		filesize -= data_read;
		if (filesize <= 0) {
			pEmptyBuffer->nFlags = OMX_BUFFERFLAG_EOS;
		}
		fprintf(stderr, "Emptying again buffer %p %d bytes, %d to go\n", pEmptyBuffer, data_read, filesize);
		err = OMX_EmptyThisBuffer(handle, pEmptyBuffer);
		waitForEmpty();
		fprintf(stderr, "Waited for empty\n");
		if (bEOS) {
			fprintf(stderr, "Exiting loop\n");
			break;
		}
	}
	fprintf(stderr, "Buffers emptied\n");
	exit(0);
}
