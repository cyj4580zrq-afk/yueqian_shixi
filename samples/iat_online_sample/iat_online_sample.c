#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"

#define BUFFER_SIZE 4096
#define FRAME_LEN 640

static void asr_from_wav(const char *audio_file, const char *session_begin_params)
{
	const char *session_id = NULL;
	char rec_result[BUFFER_SIZE] = {0};
	unsigned int total_len = 0;
	int aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
	int ep_stat = MSP_EP_LOOKING_FOR_SPEECH;
	int rec_stat = MSP_REC_STATUS_SUCCESS;
	int errcode = MSP_SUCCESS;
	FILE *f_pcm = NULL;
	char *p_pcm = NULL;
	long pcm_count = 0;
	long pcm_size = 0;
	long read_size = 0;

	if (!audio_file) return;
	f_pcm = fopen(audio_file, "rb");
	if (!f_pcm) {
		printf("\nopen [%s] failed! \n", audio_file);
		return;
	}

	fseek(f_pcm, 0, SEEK_END);
	pcm_size = ftell(f_pcm);
	fseek(f_pcm, 0, SEEK_SET);

	p_pcm = (char *)malloc(pcm_size);
	if (!p_pcm) {
		printf("\nout of memory! \n");
		fclose(f_pcm);
		return;
	}

	read_size = fread((void *)p_pcm, 1, pcm_size, f_pcm);
	if (read_size != pcm_size) {
		printf("\nread [%s] error!\n", audio_file);
		free(p_pcm);
		fclose(f_pcm);
		return;
	}

	printf("[ASR] 开始语音听写 ...\n");
	session_id = QISRSessionBegin(NULL, session_begin_params, &errcode);
	if (MSP_SUCCESS != errcode) {
		printf("QISRSessionBegin failed! error code:%d\n", errcode);
		free(p_pcm);
		fclose(f_pcm);
		return;
	}

	while (1) {
		unsigned int len = 10 * FRAME_LEN;
		int ret = 0;
		if (pcm_size < 2 * len) len = pcm_size;
		if (len <= 0) break;
		aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
		if (0 == pcm_count) aud_stat = MSP_AUDIO_SAMPLE_FIRST;
		ret = QISRAudioWrite(session_id, (const void *)&p_pcm[pcm_count], len, aud_stat, &ep_stat, &rec_stat);
		if (MSP_SUCCESS != ret) {
			printf("\nQISRAudioWrite failed! error code:%d\n", ret);
			QISRSessionEnd(session_id, "AudioWriteError");
			free(p_pcm);
			fclose(f_pcm);
			return;
		}
		pcm_count += (long)len;
		pcm_size  -= (long)len;
		if (MSP_REC_STATUS_SUCCESS == rec_stat) {
			const char *rslt = QISRGetResult(session_id, &rec_stat, 0, &errcode);
			if (MSP_SUCCESS != errcode) {
				printf("\nQISRGetResult failed! error code: %d\n", errcode);
				QISRSessionEnd(session_id, "GetResultError");
				free(p_pcm);
				fclose(f_pcm);
				return;
			}
			if (NULL != rslt) {
				unsigned int rslt_len = strlen(rslt);
				total_len += rslt_len;
				if (total_len >= BUFFER_SIZE) {
					printf("\nno enough buffer for rec_result !\n");
					QISRSessionEnd(session_id, "NoBuffer");
					free(p_pcm);
					fclose(f_pcm);
					return;
				}
				strncat(rec_result, rslt, sizeof(rec_result) - strlen(rec_result) - 1);
			}
		}
		if (MSP_EP_AFTER_SPEECH == ep_stat) break;
		usleep(200 * 1000);
	}

	errcode = QISRAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);
	if (MSP_SUCCESS != errcode) {
		printf("\nQISRAudioWrite failed! error code:%d \n", errcode);
		QISRSessionEnd(session_id, "AudioLastError");
		free(p_pcm);
		fclose(f_pcm);
		return;
	}

	while (MSP_REC_STATUS_COMPLETE != rec_stat) {
		const char *rslt = QISRGetResult(session_id, &rec_stat, 0, &errcode);
		if (MSP_SUCCESS != errcode) {
			printf("\nQISRGetResult failed, error code: %d\n", errcode);
			QISRSessionEnd(session_id, "GetResultError");
			free(p_pcm);
			fclose(f_pcm);
			return;
		}
		if (NULL != rslt) {
			unsigned int rslt_len = strlen(rslt);
			total_len += rslt_len;
			if (total_len >= BUFFER_SIZE) {
				printf("\nno enough buffer for rec_result !\n");
				QISRSessionEnd(session_id, "NoBuffer");
				free(p_pcm);
				fclose(f_pcm);
				return;
			}
			strncat(rec_result, rslt, sizeof(rec_result) - strlen(rec_result) - 1);
		}
		usleep(150 * 1000);
	}

	printf("\n[ASR] 语音听写结束\n");
	printf("[ASR] 识别文字结果: %s\n", rec_result);
	if (rec_result[0]) printf("ASR:%s\n", rec_result);

	free(p_pcm);
	fclose(f_pcm);
	QISRSessionEnd(session_id, "Normal");
}

int main(int argc, char* argv[])
{
	int ret = MSP_SUCCESS;
	const char* login_params = "appid = f7c7bd54, work_dir = .";
	const char* session_begin_params = "sub = iat, domain = iat, language = zh_cn, accent = mandarin, sample_rate = 16000, result_type = plain, result_encoding = utf8";
	const char* env_audio_file = getenv("ASR_AUDIO_FILE");
	const char* audio_file = NULL;

	if (argc >= 2 && argv[1][0]) {
		audio_file = argv[1];
	} else if (env_audio_file && env_audio_file[0]) {
		audio_file = env_audio_file;
	}

	if (!audio_file || !audio_file[0]) {
		printf("Usage: %s <wav_file>\n", argv[0]);
		printf("Or set ASR_AUDIO_FILE to the latest recorded wav path.\n");
		return -1;
	}

	ret = MSPLogin(NULL, NULL, login_params);
	if (MSP_SUCCESS != ret) {
		printf("MSPLogin failed , Error code %d.\n", ret);
		return -1;
	}

	printf("========================================================================\n");
	printf("## WSL 语音识别工具已启动 (科大讯飞 ASR)\n");
	printf("## 音频文件：%s\n", audio_file);
	printf("========================================================================\n\n");

	asr_from_wav(audio_file, session_begin_params);
	MSPLogout();
	return 0;
}
