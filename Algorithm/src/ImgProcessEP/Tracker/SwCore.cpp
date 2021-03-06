#include "SwCore.h"

#include "DspLinkCmd.h"
#include "hvmodelstore.h"
#include "tracker.h"
#include "trackerdef.h"

// 识别接口
ITracker* g_pTrackerVideo = NULL;
static CHvModelStore g_modelStore;

// 平台级接口
PLATFORM_FUCTION_LIST* g_pFuction = NULL;

void SetFuctionCallBack(PLATFORM_FUCTION_LIST* pcList)
{
	if( g_pFuction == NULL )
	{
		g_pFuction = new PLATFORM_FUCTION_LIST;
		memset(g_pFuction, 0, sizeof(PLATFORM_FUCTION_LIST));
		g_pFuction->pfDebugOutPut = pcList->pfDebugOutPut;
		g_pFuction->pfCacheWbInv = pcList->pfCacheWbInv;
		g_pFuction->pfCacheInv = pcList->pfCacheInv;
		g_pFuction->pfGetSystemTick = pcList->pfGetSystemTick;

		g_pFuction->pfDMAOpen = pcList->pfDMAOpen;
		g_pFuction->pfDMACopy = pcList->pfDMACopy;
		g_pFuction->pfDMAWait = pcList->pfDMAWait;
		g_pFuction->pfDMAClose = pcList->pfDMAClose;
	}
}

// 图片引用接口
void ImageAddRef(IMG_MEM_OPER_LOG* pLog, IReferenceComponentImage* pRefImage)
{
	if ( pLog && pRefImage )
	{
		pLog->rgOperInfo[pLog->nCount].pMemAddr = pRefImage;
		pLog->rgOperInfo[pLog->nCount++].nOperFlag = IOF_ADD_REF;
	}
}

void ImageRelease(IMG_MEM_OPER_LOG* pLog, IReferenceComponentImage* pRefImage)
{
	if ( pLog && pRefImage )
	{
		pLog->rgOperInfo[pLog->nCount].pMemAddr = pRefImage;
		pLog->rgOperInfo[pLog->nCount++].nOperFlag = IOF_RELEASE;
	}
}

// 视频流初始化接口
int InitDspCmdLib_Video(int nPlateRecogParamIndex, int nLastLightType, int nLastPulseLevel,int nLastCplStatus, void* pvParam)
{
	DEBUG_OUTPUT pfDebugOutPut = NULL;
	if( NULL != g_pFuction )
	{
		pfDebugOutPut = g_pFuction->pfDebugOutPut;
	}
	HRESULT hr = S_OK;
	// 设置全局参数
	char szMsg[256];

	LoadPlateRecogParam(nPlateRecogParamIndex);
	if (NULL == g_pTrackerVideo)
	{
		CreateTracker_Video(&g_pTrackerVideo);
		if (g_pTrackerVideo)
		{
			using namespace HvCore;
			IHvModel* pHvModel;
			g_modelStore.QueryInterface(IID_IHvModel, (void**)&pHvModel);
			g_pTrackerVideo->SetHvModel(&g_modelStore);
			hr = g_pTrackerVideo->InitTracker((TRACKER_CFG_PARAM*)pvParam);
			g_pTrackerVideo->SetFirstLightType((LIGHT_TYPE)nLastLightType);
			g_pTrackerVideo->SetFirstPulseLevel(nLastPulseLevel);
			g_pTrackerVideo->SetFirstCplStatus(nLastCplStatus);
		}
		if( g_pTrackerVideo == NULL ) return -1;
		if( S_OK != hr ) return -2;
		return 0;
	}
	else
	{
		return 1;
	}
}

int ModifyVideoParam(void* pvParam, int iSize, void* pvRespond, int iRespondSize)
{
	if( NULL == g_pTrackerVideo )
	{
		return 0;
	}

	DEBUG_OUTPUT pfDebugOutPut = NULL;
	if( NULL != g_pFuction )
	{
		pfDebugOutPut = g_pFuction->pfDebugOutPut;
	}

	// 四字节的共享内存地址，目前保持与水星一致。
	char szMsg[256];

	if(iSize != 4 || NULL == pvParam )
	{
		return -1;
	}

	// 解释输入输出
	DSPLinkBuffer *input = NULL;
	DSPLinkBuffer *output = NULL;
	DSPLinkBuffer *msg = (DSPLinkBuffer *)(*((int*)pvParam));
	input  = &msg[0];
	output = &msg[1];

	if( NULL != pfDebugOutPut )
	{
		sprintf(szMsg, "<ModifyvideoParam>input:%08x,%08x, %d. output:%08x,%08x, %d.\n",
				input->addr, input->phys, input->len, output->addr, output->phys, output->len);
		pfDebugOutPut(szMsg);
	}


	unsigned char* pbInbuf = (unsigned char*)input->phys;
	unsigned char* pbOutbuf = (unsigned char*)output->phys;
	int iOutputSize = output->len;

	// 解释命令
	DSP_CMD_ID nCmdId = DSP_CMD_ID(*((DWORD32*)pbInbuf));
	if (sizeof(DSP_RESPOND_HEAD) > iOutputSize)
	{
		return -2;
	}

	// 只处理指定命令
	if( DCI_MODIFY_VIDEO_PARAM != nCmdId )
	{
		return -3;
	}

	DSP_RESPOND_HEAD* pRespondHead = (DSP_RESPOND_HEAD*)pbOutbuf;
	pRespondHead->dwCmdId = nCmdId;
	pRespondHead->iResult = -1;
	pRespondHead->iAppendDataSize = 0;

	// 调用识别初始化接口
	unsigned char* pbPos = pbInbuf + 4;
	int* pnParamSize = (int*)pbPos;

	if( NULL != pfDebugOutPut )
	{
		sprintf(szMsg, "<ModifyvideoParam>RECOGER_PARAM size:%d %d.\n",
				*pnParamSize, sizeof(TRACKER_CFG_PARAM));
		pfDebugOutPut(szMsg);
	}

	int iRet = 0;
	// 判断参数长度是否正确
	if (*pnParamSize == sizeof(TRACKER_CFG_PARAM))
	{
		pbPos += 4;
		TRACKER_CFG_PARAM* pParam = (TRACKER_CFG_PARAM*)pbPos;
		// todo.
		// 是否还需要进行时间的校正
		//RectifySystemTick(pParam->dwArmTick);
		if( g_pTrackerVideo == NULL )
		{
			pRespondHead->iResult = S_OK;
		}
		else
		{
			pRespondHead->iResult = g_pTrackerVideo->ModifyTracker(pParam);
		}


		if( NULL != pfDebugOutPut )
		{
			sprintf(szMsg, "<ModifyvideoParam>ModifyTracker ret:%08x.\n",
					pRespondHead->iResult);
			pfDebugOutPut(szMsg);
		}

		if (pRespondHead->iResult == S_OK)
		{
			sprintf(
				pRespondHead->szErrorInfo,
				"Modify video recoger ok.\n"
				);
		}
		else
		{
			// 输出错误信息
			sprintf(
				pRespondHead->szErrorInfo,
				"Modify is failed!!!\n"
				);
			iRet = -4;
		}
	}
	else
	{
		// 输出错误信息
		sprintf(
			pRespondHead->szErrorInfo,
			"Modify video recoger failed, the size of param error!!!\n"
			);
		iRet = -5;
	}


	if( NULL != g_pFuction && g_pFuction->pfCacheWbInv != NULL )
	{
		g_pFuction->pfCacheWbInv(pbOutbuf, iOutputSize, 0xFF, 1);
	}

	return iRet;
}

int InitVideoParam(void* pvParam, int iSize, void* pvRespond, int iRespondSize)
{
	DEBUG_OUTPUT pfDebugOutPut = NULL;
	if( NULL != g_pFuction )
	{
		pfDebugOutPut = g_pFuction->pfDebugOutPut;
	}

	// 四字节的共享内存地址，目前保持与水星一致。
	char szMsg[256];

	if(iSize != 4 || NULL == pvParam )
	{
		return -1;
	}

	// 解释输入输出
	DSPLinkBuffer *input = NULL;
	DSPLinkBuffer *output = NULL;
	DSPLinkBuffer *msg = (DSPLinkBuffer *)(*((int*)pvParam));
	input  = &msg[0];
	output = &msg[1];

	if( NULL != pfDebugOutPut )
	{
		sprintf(szMsg, "<InitvideoParam>input:%08x,%08x, %d. output:%08x,%08x, %d.\n",
				input->addr, input->phys, input->len, output->addr, output->phys, output->len);
		pfDebugOutPut(szMsg);
	}


	unsigned char* pbInbuf = (unsigned char*)input->phys;
	unsigned char* pbOutbuf = (unsigned char*)output->phys;
//	int iInputSize = input->len;
	int iOutputSize = output->len;

	// 解释命令
	DSP_CMD_ID nCmdId = DSP_CMD_ID(*((DWORD32*)pbInbuf));
	if (sizeof(DSP_RESPOND_HEAD) > iOutputSize)
	{
		return -2;
	}

	// 只处理指定命令
	// todo.
	// 修改与初始化现是同一命令
	if( DCI_INIT_VIDEO_RECOGER != nCmdId
			&& DCI_MODIFY_VIDEO_PARAM != nCmdId )
	{
		return -3;
	}

	// 如果是修改参数命令
	if( DCI_MODIFY_VIDEO_PARAM == nCmdId )
	{
		return ModifyVideoParam(pvParam, iSize, pvRespond, iRespondSize);
	}
	// 清空输出内存
	memset(pbOutbuf, 0, iOutputSize);
	DSP_RESPOND_HEAD* pRespondHead = (DSP_RESPOND_HEAD*)pbOutbuf;
	pRespondHead->dwCmdId = nCmdId;
	pRespondHead->iResult = -1;
	pRespondHead->iAppendDataSize = 0;

	// 调用识别初始化接口
	unsigned char* pbPos = pbInbuf + 4;
	int* pnParamSize = (int*)pbPos;

	if( NULL != pfDebugOutPut )
	{
		sprintf(szMsg, "<InitvideoParam>RECOGER_PARAM size:%d %d.\n",
				*pnParamSize, sizeof(INIT_VIDEO_RECOGER_PARAM));
		pfDebugOutPut(szMsg);
	}

	int iRet = 0;
	// 判断参数长度是否正确
	if (*pnParamSize == sizeof(INIT_VIDEO_RECOGER_PARAM))
	{
		pbPos += 4;
		INIT_VIDEO_RECOGER_PARAM* pParam = (INIT_VIDEO_RECOGER_PARAM*)pbPos;
		// todo.
		// 是否还需要进行时间的校正
		//RectifySystemTick(pParam->dwArmTick);
		pRespondHead->iResult = InitDspCmdLib_Video(pParam->nPlateRecogParamIndex,
			pParam->nLastLightType,
			pParam->nLastPulseLevel,
			pParam->nLastCplStatus,
			&(pParam->cTrackerCfgParam)
			);

		if( NULL != pfDebugOutPut )
		{
			sprintf(szMsg, "<InitvideoParam>InitDspCmdLib_Video ret:%08x.\n",
					pRespondHead->iResult);
			pfDebugOutPut(szMsg);
		}

		if (pRespondHead->iResult == S_OK)
		{
			sprintf(
				pRespondHead->szErrorInfo,
				"Init video recoger ok.\n"
				);
			RectifySystemTick(pParam->dwArmTick);
		}
		else
		{
			// 输出错误信息
			sprintf(
				pRespondHead->szErrorInfo,
				"InitDspCmdLib_Video is failed!!!\n"
				);
			iRet = -4;
		}
	}
	else
	{
		// 输出错误信息
		sprintf(
			pRespondHead->szErrorInfo,
			"Init video recoger failed, the size of param error!!!\n"
			);
		iRet = -5;
	}


	if( NULL != g_pFuction && g_pFuction->pfCacheWbInv != NULL )
	{
		g_pFuction->pfCacheWbInv(pbOutbuf, iOutputSize, 0xFF, 1);
	}

	return iRet;
}

int ProcessOneFrame(void* pvParam, int iSize, void* pvRespond, int iRespondSize)
{
	//ganzz
	static int s_LastEnd = 0;
	unsigned int _nS1 = GetSystemTick(); //dbg


	DEBUG_OUTPUT pfDebugOutPut = NULL;
	if( NULL != g_pFuction )
	{
		pfDebugOutPut = g_pFuction->pfDebugOutPut;
	}
	F_CACHE_INV pfCacheInv = NULL;
	if( NULL != g_pFuction )
	{
		pfCacheInv = g_pFuction->pfCacheInv;
	}
	F_CACHE_INV pfCacheWbInv = NULL;
	if( NULL != g_pFuction )
	{
		pfCacheWbInv = g_pFuction->pfCacheWbInv;
	}

	//gzz
	{
	char _szBuf[256];
	//sprintf(_szBuf, "CORE_Last:%d\n",
	//		GetSystemTick() - s_LastEnd);
	//pfDebugOutPut(_szBuf);
	}

	char szMsg[256];

	// 四字节的共享内存地址，目前保持与水星一致。
	if(iSize != 4 || NULL == pvParam)
	{
		return -1;
	}

	// 解释输入输出
	DSPLinkBuffer *input = NULL;
	DSPLinkBuffer *output = NULL;
	DSPLinkBuffer *msg = (DSPLinkBuffer *)(*((int*)pvParam));
	input  = &msg[0];
	output = &msg[1];

	unsigned char* pbInbuf = (unsigned char*)input->phys;
	unsigned char* pbOutbuf = (unsigned char*)output->phys;
	int iInputSize = input->len;
	int iOutputSize = output->len;

	// 读之前先置CACHE无效
	if(NULL != pfCacheInv)
	{
		pfCacheInv(pbInbuf, iInputSize, CACHE_YTPE_ALL, 1);
	}

	// 解释命令
	DSP_CMD_ID nCmdId = DSP_CMD_ID(*((DWORD32*)pbInbuf));

	if (sizeof(DSP_RESPOND_HEAD) > iOutputSize)
	{
		return -2;
	}
	// 清空输出内存
	// ganzz，只要清头部即可，其它需要数据请逐个清空
	memset(pbOutbuf, 0, sizeof(DSP_RESPOND_HEAD)/*iOutputSize*/);
	DSP_RESPOND_HEAD* pRespondHead = (DSP_RESPOND_HEAD*)pbOutbuf;
	pRespondHead->dwCmdId = nCmdId;
	pRespondHead->iResult = -1;
	pRespondHead->iAppendDataSize = 0;

	int iRet = 0;
	unsigned char* pbPos = pbInbuf + 4;
	int* pnParamSize = (int*)pbPos;
	pbPos += 4;
	// 判断参数长度是否正确
	PROCESS_ONE_FRAME_PARAM* pParam = (PROCESS_ONE_FRAME_PARAM*)pbPos;
	// todo.
	// 时间校正是否还需要。
	RectifySystemTick(pParam->dwCurTick);
	pbPos += *pnParamSize;
	int* pnDataSize = (int*)pbPos;
	pbPos += 4;
	PROCESS_ONE_FRAME_DATA* pData = (PROCESS_ONE_FRAME_DATA*)pbPos;

	// 只处理指定命令
	if( DCI_PROCESS_ONE_FRAME != nCmdId )
	{
		strcpy(pRespondHead->szErrorInfo, "<1234>DCI_PROCESS_ONE_FRAME != nCmdId.");

		// 写完后要WB且置CACHE无效。
		if( pfCacheWbInv != NULL )
		{
			pfCacheWbInv(pbOutbuf, iOutputSize, 0xFF, 1);
		}

		return -3;
	}

	/*
	if( NULL != pfDebugOutPut )
	{
		sprintf(szMsg, "<process123>paramsize:%d,%d.datasize:%d,%d.",
				(*pnParamSize), sizeof(PROCESS_ONE_FRAME_PARAM),
				(*pnDataSize), sizeof(PROCESS_ONE_FRAME_DATA));
		pfDebugOutPut(szMsg);
	}*/

	// 读之前先置CACHE无效
	if(NULL != pfCacheInv)
	{
		pfCacheInv(pData->cSyncDetData.pbData, pData->cSyncDetData.nLen, CACHE_YTPE_ALL, 1);
	}

	if (*pnParamSize == sizeof(PROCESS_ONE_FRAME_PARAM) && *pnDataSize == sizeof(PROCESS_ONE_FRAME_DATA))
	{
		PROCESS_ONE_FRAME_RESPOND* pProcessRespond = (PROCESS_ONE_FRAME_RESPOND*)(pbOutbuf + sizeof(DSP_RESPOND_HEAD));
		memset(pProcessRespond, 0, sizeof(PROCESS_ONE_FRAME_RESPOND));
		pRespondHead->iAppendDataSize = sizeof(PROCESS_ONE_FRAME_RESPOND);
		pRespondHead->iResult = E_FAIL;

		// zhaopy
		// todo.
		// 强制设成 HV_IMAGE_YUV_420
		pData->hvImageYuv.nImgType = HV_IMAGE_YUV_420;

		if (pData->hvImageYuv.nImgType == HV_IMAGE_JPEG)
		{
			//不支持JPEG识别。
			iRet = -4;
		}
		else if (pData->hvImageYuv.nImgType == HV_IMAGE_BT1120
				|| pData->hvImageYuv.nImgType == HV_IMAGE_YUV_420)
		{
			// 图片抽场
			//todo.
			// 600W 已抽场。
			if(pData->hvImageYuv.iWidth < 2800 )
			{
				pData->hvImageYuv.iStrideWidth[0] = (pData->hvImageYuv.iStrideWidth[0] << 1);
				pData->hvImageYuv.iStrideWidth[1] = (pData->hvImageYuv.iStrideWidth[1] << 1);
				pData->hvImageYuv.iHeight >>= 1;
			}
			else if( pData->hvImageYuv.iHeight * 2 > pData->hvImageYuv.iWidth ) // 没抽场的图
			{
				pData->hvImageYuv.iStrideWidth[0] = (pData->hvImageYuv.iStrideWidth[0] << 1);
				pData->hvImageYuv.iStrideWidth[1] = (pData->hvImageYuv.iStrideWidth[1] << 1);
				pData->hvImageYuv.iHeight >>= 1;
			}
			pRespondHead->iResult = S_OK;
		}
		else
		{
			pRespondHead->iResult = S_FALSE;
			iRet = -4;
		}

		//gzz
		/*{
		char _szBuf[256];
		sprintf(_szBuf, "CORE_T1:%d\n",
				GetSystemTick() - _nS1);
		pfDebugOutPut(_szBuf);
		}*/

		if (pRespondHead->iResult == S_OK && g_pTrackerVideo)
		{
			// todo.
			pRespondHead->iResult = g_pTrackerVideo->ProcessOneFrame(pParam, pData, pProcessRespond);
			//pRespondHead->iResult = S_OK;
		}

		// 输出错误信息
		memcpy(pRespondHead->szErrorInfo, g_szDebugInfo, 256);

		// 车牌小图的内存要更新cache
		for(int i = 0; i < pProcessRespond->cTrigEvent.iCarLeftCount; ++i)
		{
			for(int j = 0; j < 3; ++j)
			{
				if( 0 != pData->rghvImageSmall[i].rgImageData[j].phys )
				{
					pfCacheWbInv((void*)pData->rghvImageSmall[i].rgImageData[j].phys,
						pData->rghvImageSmall[i].rgImageData[j].len,  0xFF, 1);
				}
			}
			for(int j = 0; j < 3; ++j)
			{
				if( 0 != pData->rghvImageBin[i].rgImageData[j].phys )
				{
					pfCacheWbInv((void*)pData->rghvImageBin[i].rgImageData[j].phys,
						pData->rghvImageBin[i].rgImageData[j].len,  0xFF, 1);
				}
			}
		}

	}
	else
	{
		sprintf(pRespondHead->szErrorInfo, "\nDSPLink protocol not match!\n");
	}

	pRespondHead->szErrorInfo[255] = 0;

	// 写完后要WB且置CACHE无效。
	if( pfCacheWbInv != NULL )
	{
		pfCacheWbInv(pbOutbuf, iOutputSize, 0xFF, 1);
	}

	s_LastEnd = GetSystemTick();

	return iRet;
}
