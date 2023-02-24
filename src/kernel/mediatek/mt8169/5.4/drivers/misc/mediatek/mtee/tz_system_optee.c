// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/tee_drv.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"
#include "kree/system.h"
#include "kree/mem.h"

KREE_SESSION_HANDLE MTEE_SESSION_HANDLE_SYSTEM;

static struct tee_context *tee_ctx;
#define TEEC_PAYLOAD_REF_COUNT 4

/* match func always return "matched", works if there is only one TEE. */
static int _KREE_dev_match(struct tee_ioctl_version_data *t, const void *v)
{
	return 1;
}
static int mtee_to_optee_arg(uint32_t paramTypes,
				struct tee_param params[TEEC_PAYLOAD_REF_COUNT],
				union MTEEC_PARAM *oparam)
{
	int i;
	int type;

	for (i = 0; i < TEEC_PAYLOAD_REF_COUNT; i++) {
		type = (paramTypes >> i * 8) & 0xff;
		switch (type) {
		case TZPT_NONE:
			params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
		break;
		case TZPT_VALUE_INPUT:
			params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
			params[i].u.value.a = oparam[i].value.a;
			params[i].u.value.b = oparam[i].value.b;
		break;
		case TZPT_VALUE_INOUT:
			params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
			params[i].u.value.a = oparam[i].value.a;
			params[i].u.value.b = oparam[i].value.b;
		break;
		case TZPT_VALUE_OUTPUT:
			params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
		break;
		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_MEMREF_OUTPUT:
			if (type == TZPT_MEMREF_INPUT)
				params[i].attr =
					TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
			else if (type == TZPT_MEMREF_INOUT)
				params[i].attr =
					TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
			else
				params[i].attr =
					TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;

			params[i].u.memref.shm =
			tee_shm_get_from_id(tee_ctx, oparam[i].memref.handle);
			params[i].u.memref.size = oparam[i].memref.size;
			params[i].u.memref.shm_offs = oparam[i].memref.offset;
		break;
		default:
			pr_warn("TZPT_MEM_* is not supported!!");
		}
	}
	return 0;
}

static int mtee_from_optee_arg(uint32_t paramTypes,
				union MTEEC_PARAM *oparam,
				struct tee_param params[TEEC_PAYLOAD_REF_COUNT])
{
	int i;
	int type;

	for (i = 0; i < TEEC_PAYLOAD_REF_COUNT; i++) {
		type = (paramTypes >> i * 8) & 0xff;
		switch (type) {
		case TZPT_NONE:
		case TZPT_VALUE_INPUT:
		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_MEMREF_OUTPUT:
		/* nothing to do */
		break;
		case TZPT_VALUE_INOUT:
		case TZPT_VALUE_OUTPUT:
			oparam[i].value.a = params[i].u.value.a;
			oparam[i].value.b = params[i].u.value.b;
		break;
		default:
			pr_warn("TZPT_MEM_* is not supported!!");
		}
	}
	return 0;
}

int KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
			      uint32_t paramTypes, union MTEEC_PARAM oparam[4])
{
	int rc;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];

	if (tee_ctx == NULL)
		return TZ_RESULT_ERROR_GENERIC;

	memset(&arg, 0, sizeof(arg));
	arg.num_params = TEEC_PAYLOAD_REF_COUNT;
	arg.session = (u32)handle;
	arg.func = command;

	memset(params, 0, sizeof(params));
	mtee_to_optee_arg(paramTypes, params, oparam);
	rc = tee_client_invoke_func(tee_ctx, &arg, params);

	if (rc) {
		pr_err("%s(): rc = %d\n", __func__, rc);
		return TZ_RESULT_ERROR_GENERIC;
	}
	if (arg.ret != 0) {
		pr_err("%s(): ret %d, orig %d", __func__,
			arg.ret, arg.ret_origin);
		return TZ_RESULT_ERROR_GENERIC;
	}
	mtee_from_optee_arg(paramTypes, oparam, params);

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_TeeServiceCall);

static void str_to_uuid(const char *str, uint8_t *uuid)
{
	int b, l, e, t;

	for (t = 0, e = 0, l = 0, b = 16;
	     str[l] != '\0' && l < 36 && e < 16; l++) {
		if (str[l] >= '0' && str[l] <= '9')
			t += (str[l] - '0')*b;
		else if (str[l] >= 'a' && str[l] <= 'f')
			t += (str[l] - 'a' + 10)*b;
		else if (str[l] >= 'A' && str[l] <= 'F')
			t += (str[l] - 'A' + 10)*b;
		else {
			/* char not recognize, skip */
			b = 16;
			continue;
		}

		if (b == 1) {
			uuid[e++] = t;
			t = 0;
			b = 16;
		} else
			b = 1;
	}
}

int KREE_InitTZ(void)
{
	int rc;
	struct tee_ioctl_open_session_arg osarg;

	tee_ctx = tee_client_open_context(NULL, _KREE_dev_match,
							NULL, NULL);
	if (IS_ERR(tee_ctx)) {
		pr_err("%s() failed err %ld", __func__, PTR_ERR(tee_ctx));
		return TZ_RESULT_ERROR_GENERIC;
	}

	memset(&osarg, 0, sizeof(osarg));
	osarg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	str_to_uuid(PTA_SYSTEM_UUID_STRING, osarg.uuid);
	rc = tee_client_open_session(tee_ctx, &osarg, NULL);
	if (rc || osarg.ret) {
		pr_err("open_session failed err %d, ret=%d", rc, osarg.ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	MTEE_SESSION_HANDLE_SYSTEM = osarg.session;

	return TZ_RESULT_SUCCESS;
}

int KREE_CreateSession(const char *ta_uuid, KREE_SESSION_HANDLE *pHandle)
{
	int rc;
	struct tee_ioctl_open_session_arg arg;
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];

	if (tee_ctx == NULL)
		return TZ_RESULT_ERROR_GENERIC;

	memset(&arg, 0, sizeof(arg));
	arg.num_params = TEEC_PAYLOAD_REF_COUNT;
	arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	str_to_uuid(ta_uuid, arg.uuid);
	memset(params, 0, sizeof(params));
	rc = tee_client_open_session(tee_ctx, &arg, params);

	if (rc)
		return TZ_RESULT_ERROR_GENERIC;

	if (arg.ret != 0) {
		pr_err("ret=%d", arg.ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	*pHandle = arg.session;
	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_CreateSession);

int KREE_CreateSessionWithTag(const char *ta_uuid,
					KREE_SESSION_HANDLE *pHandle,
					const char *tag)
{
	return KREE_CreateSession(ta_uuid, pHandle);
}
EXPORT_SYMBOL(KREE_CreateSessionWithTag);

int KREE_CloseSession(KREE_SESSION_HANDLE handle)
{
	int rc;

	if (tee_ctx == NULL)
		return TZ_RESULT_ERROR_GENERIC;

	rc = tee_client_close_session(tee_ctx, (u32)handle);
	if (rc != 0)
		return TZ_RESULT_ERROR_GENERIC;
	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_CloseSession);

int KREE_RegisterSharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *shm_handle,
					struct KREE_SHAREDMEM_PARAM *param)
{
	int rc;

	if (tee_ctx == NULL)
		return TZ_RESULT_ERROR_GENERIC;

	rc = tee_client_register_shm(tee_ctx,
					(unsigned long)param->buffer,
					param->size,
					shm_handle);

	if (rc != 0) {
		pr_err("%s: rc = %d\n", __func__, rc);
		return TZ_RESULT_ERROR_GENERIC;
	}

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_RegisterSharedmem);

int KREE_RegisterSharedmemWithTag(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *shm_handle,
					struct KREE_SHAREDMEM_PARAM *param,
					const char *tag)
{
	return KREE_RegisterSharedmem(session, shm_handle, param);
}
EXPORT_SYMBOL(KREE_RegisterSharedmemWithTag);

int KREE_UnregisterSharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE shm_handle)
{
	int rc;

	if (tee_ctx == NULL)
		return TZ_RESULT_ERROR_GENERIC;

	rc = tee_client_unregister_shm(tee_ctx, shm_handle);
	if (rc != 0)
		return TZ_RESULT_ERROR_GENERIC;

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_UnregisterSharedmem);

#include "tz_cross/tz_error_strings.h"

const char *TZ_GetErrorString(int res)
{
	return _TZ_GetErrorString(res);
}
EXPORT_SYMBOL(TZ_GetErrorString);

MODULE_LICENSE("GPL v2");
