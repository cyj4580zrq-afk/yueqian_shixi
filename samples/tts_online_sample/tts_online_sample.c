/*
* 语音合成（Text To Speech，TTS）CLI
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "qtts.h"
#include "msp_cmn.h"
#include "msp_errors.h"

typedef struct _wave_pcm_hdr
{
	char            riff[4];
	int		size_8;
	char            wave[4];
	char            fmt[4];
	int		fmt_size;
	short int       format_tag;
	short int       channels;
	int		samples_per_sec;
	int		avg_bytes_per_sec;
	short int       block_align;
	short int       bits_per_sample;
	char            data[4];
	int		data_size;
} wave_pcm_hdr;

wave_pcm_hdr default_wav_hdr =
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0
};

int text_to_speech(const char* src_text, const char* des_path, const char* params)
{
	int ret = -1;
	FILE* fp = NULL;
	const char* sessionID = NULL;
	unsigned int audio_len = 0;
	wave_pcm_hdr wav_hdr = default_wav_hdr;
	int synth_status = MSP_TTS_FLAG_STILL_HAVE_DATA;

	if (NULL == src_text || NULL == des_path)
	{
		printf("params is error!\n");
		return ret;
	}
	fp = fopen(des_path, "wb");
	if (NULL == fp)
	{
		printf("open %s error.\n", des_path);
		return ret;
	}
	sessionID = QTTSSessionBegin(params, &ret);
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSSessionBegin failed, error code: %d.\n", ret);
		fclose(fp);
		return ret;
	}
	ret = QTTSTextPut(sessionID, src_text, (unsigned int)strlen(src_text), NULL);
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSTextPut failed, error code: %d.\n", ret);
		QTTSSessionEnd(sessionID, "TextPutError");
		fclose(fp);
		return ret;
	}
	printf("正在合成 ...\n");
	fwrite(&wav_hdr, sizeof(wav_hdr), 1, fp);
	while (1)
	{
		const void* data = QTTSAudioGet(sessionID, &audio_len, &synth_status, &ret);
		if (MSP_SUCCESS != ret) break;
		if (NULL != data)
		{
			fwrite(data, audio_len, 1, fp);
			wav_hdr.data_size += audio_len;
		}
		if (MSP_TTS_FLAG_DATA_END == synth_status) break;
		usleep(150 * 1000);
	}
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSAudioGet failed, error code: %d.\n", ret);
		QTTSSessionEnd(sessionID, "AudioGetError");
		fclose(fp);
		return ret;
	}
	wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);
	fseek(fp, 4, 0);
	fwrite(&wav_hdr.size_8, sizeof(wav_hdr.size_8), 1, fp);
	fseek(fp, 40, 0);
	fwrite(&wav_hdr.data_size, sizeof(wav_hdr.data_size), 1, fp);
	fclose(fp);
	fp = NULL;
	ret = QTTSSessionEnd(sessionID, "Normal");
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSSessionEnd failed, error code: %d.\n", ret);
	}
	printf("[TTS] 语音合成成功，输出路径：%s\n", des_path);
	return ret;
}

int main(int argc, char* argv[])
{
	int ret = MSP_SUCCESS;
	const char* login_params = "appid = f7c7bd54, work_dir = .";
	const char* session_begin_params = "voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
	const char* filename = (argc >= 3 && argv[2][0]) ? argv[2] : "tts_sample.wav";
	const char* text = (argc >= 2 && argv[1][0]) ? argv[1] : "科大讯飞作为智能语音技术提供商，在智能语音技术领域有着长期的研究积累，并在中文语音合成、语音识别、口语评测等多项技术上拥有技术成果。科大讯飞是我国以语音技术为产业化方向的国家863计划产业化基地。";

	ret = MSPLogin(NULL, NULL, login_params);
	if (MSP_SUCCESS != ret)
	{
		printf("MSPLogin failed, error code: %d.\n", ret);
		return -1;
	}

	printf("开始合成 ...\n");
	ret = text_to_speech(text, filename, session_begin_params);
	if (MSP_SUCCESS != ret)
	{
		printf("text_to_speech failed, error code: %d.\n", ret);
	}
	printf("合成完毕\n");
	printf("TTS:%s\n", filename);

	MSPLogout();
	return 0;
}
