// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2016, Linaro Limited
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include "tee_client_api.h"

static int tee_dev_match(struct tee_ioctl_version_data *t
		, const void *v)
{
#if IS_ENABLED(CONFIG_OPTEE)
	if (t->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
#endif
	return 0;
}

TEEC_Result TEEC_InitializeContext(const char *name, struct TEEC_Context *ctx)
{
	if (!ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	ctx->ctx = tee_client_open_context(NULL, tee_dev_match, NULL, NULL);
	if (IS_ERR(ctx))
		return TEEC_ERROR_ITEM_NOT_FOUND;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(TEEC_InitializeContext);

void TEEC_FinalizeContext(struct TEEC_Context *ctx)
{
	if (ctx)
		tee_client_close_context(ctx->ctx);
}
EXPORT_SYMBOL(TEEC_FinalizeContext);

static TEEC_Result _TEEC_PreProcess_tmpref(struct TEEC_Context *ctx,
			uint32_t param_type,
			struct TEEC_TempMemoryReference *tmpref,
			struct tee_param *param,
			struct TEEC_SharedMemory *shm)
{
	TEEC_Result res;

	switch (param_type) {
	case TEEC_MEMREF_TEMP_INPUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
		shm->flags = TEEC_MEM_INPUT;
		break;
	case TEEC_MEMREF_TEMP_OUTPUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
		shm->flags = TEEC_MEM_OUTPUT;
		break;
	case TEEC_MEMREF_TEMP_INOUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
		shm->flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
		break;
	default:
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	shm->size = tmpref->size;

	res = TEEC_AllocateSharedMemory(ctx, shm);
	if (res != TEEC_SUCCESS)
		return res;

	memcpy(shm->buffer, tmpref->buffer, tmpref->size);
	param->u.memref.shm = shm->pteeshm;
	param->u.memref.size = tmpref->size;
	param->u.memref.shm_offs = 0;
	return TEEC_SUCCESS;
}

static TEEC_Result _TEEC_PreProcess_whole(
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_param *param)
{
	const uint32_t inout = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	uint32_t flags = memref->parent->flags & inout;
	struct TEEC_SharedMemory *shm;

	if (flags == inout)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	else if (flags & TEEC_MEM_INPUT)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	else if (flags & TEEC_MEM_OUTPUT)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	else
		return TEEC_ERROR_BAD_PARAMETERS;

	shm = memref->parent;
	param->u.memref.shm = shm->pteeshm;
	param->u.memref.size = shm->size;
	param->u.memref.shm_offs = 0;
	return TEEC_SUCCESS;
}

static TEEC_Result _TEEC_PreProcess_partial(uint32_t param_type,
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_param *param)
{
	uint32_t req_shm_flags;
	struct TEEC_SharedMemory *shm;

	switch (param_type) {
	case TEEC_MEMREF_PARTIAL_INPUT:
		req_shm_flags = TEEC_MEM_INPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
		break;
	case TEEC_MEMREF_PARTIAL_OUTPUT:
		req_shm_flags = TEEC_MEM_OUTPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
		break;
	case TEEC_MEMREF_PARTIAL_INOUT:
		req_shm_flags = TEEC_MEM_OUTPUT | TEEC_MEM_INPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
		break;
	default:
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	shm = memref->parent;

	if ((shm->flags & req_shm_flags) != req_shm_flags)
		return TEEC_ERROR_BAD_PARAMETERS;

	param->u.memref.shm = shm->pteeshm;
	param->u.memref.shm_offs = memref->offset;
	param->u.memref.size = memref->size;
	return TEEC_SUCCESS;
}

static void _TEEC_PostProcess_tmpref(uint32_t param_type,
			struct TEEC_TempMemoryReference *tmpref,
			struct tee_param *param,
			struct TEEC_SharedMemory *shm)
{
	if (param_type != TEEC_MEMREF_TEMP_INPUT) {
		if (param->u.memref.size <= tmpref->size && tmpref->buffer)
			memcpy(tmpref->buffer, shm->buffer,
			       param->u.memref.size);
	}
}

static void _TEEC_PostProcess_Operation(struct TEEC_Operation *operation,
			struct tee_param *params,
			struct TEEC_SharedMemory *shms)
{
	size_t n;

	if (!operation)
		return;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		uint32_t param_type;

		param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, n);
		switch (param_type) {
		case TEEC_VALUE_INPUT:
			break;
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			operation->params[n].value.a = params[n].u.value.a;
			operation->params[n].value.b = params[n].u.value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			_TEEC_PostProcess_tmpref(param_type,
				&operation->params[n].tmpref, params + n,
				shms + n);
			break;
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		default:
			break;
		}
	}
}

static void _TEEC_Free_tmprefs(struct TEEC_Operation *operation,
			struct TEEC_SharedMemory *shms)
{
	size_t n;

	if (!operation)
		return;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		switch (TEEC_PARAM_TYPE_GET(operation->paramTypes, n)) {
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			TEEC_ReleaseSharedMemory(shms + n);
			break;
		default:
			break;
		}
	}
}

static void uuid_to_octets(uint8_t d[TEE_IOCTL_UUID_LEN],
				const struct TEEC_UUID *s)
{
	d[0] = s->timeLow >> 24;
	d[1] = s->timeLow >> 16;
	d[2] = s->timeLow >> 8;
	d[3] = s->timeLow;
	d[4] = s->timeMid >> 8;
	d[5] = s->timeMid;
	d[6] = s->timeHiAndVersion >> 8;
	d[7] = s->timeHiAndVersion;
	memcpy(d + 8, s->clockSeqAndNode, sizeof(s->clockSeqAndNode));
}

static TEEC_Result _TEEC_PreProcess_Operation(struct TEEC_Context *ctx,
			struct TEEC_Operation *operation,
			struct tee_param params[TEEC_CONFIG_PAYLOAD_REF_COUNT],
			struct TEEC_SharedMemory *shms)
{
	TEEC_Result res;
	size_t n;

	memset(params, 0, sizeof(struct tee_param) *
			  TEEC_CONFIG_PAYLOAD_REF_COUNT);

	if (!operation)
		return TEEC_SUCCESS;

	memset(shms, 0, sizeof(struct TEEC_SharedMemory) *
			TEEC_CONFIG_PAYLOAD_REF_COUNT);

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		uint32_t param_type;

		param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, n);
		switch (param_type) {
		case TEEC_NONE:
			params[n].attr = param_type;
			break;
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			params[n].attr = param_type;
			params[n].u.value.a = operation->params[n].value.a;
			params[n].u.value.b = operation->params[n].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			res = _TEEC_PreProcess_tmpref(ctx, param_type,
				&operation->params[n].tmpref, params + n,
				shms + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		case TEEC_MEMREF_WHOLE:
			res = _TEEC_PreProcess_whole(
					&operation->params[n].memref,
					params + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			res = _TEEC_PreProcess_partial(param_type,
				&operation->params[n].memref, params + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		default:
			return TEEC_ERROR_BAD_PARAMETERS;
		}
	}

	return TEEC_SUCCESS;
}

TEEC_Result TEEC_OpenSession(struct TEEC_Context *ctx,
			struct TEEC_Session *session,
			const struct TEEC_UUID *destination,
			uint32_t connection_method,
			const void *connection_data,
			struct TEEC_Operation *operation,
			uint32_t *ret_origin)
{
	struct tee_ioctl_open_session_arg osarg;
	struct tee_param params[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	TEEC_Result res;
	uint32_t eorig;
	struct TEEC_SharedMemory shms[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	int rc;

	(void)&connection_data;

	if (!ctx || !session) {
		eorig = TEEC_ORIGIN_API;
		res = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	res = _TEEC_PreProcess_Operation(ctx, operation, params, shms);
	if (res != TEEC_SUCCESS) {
		eorig = TEEC_ORIGIN_API;
		goto out_free_tmprefs;
	}

	memset(&osarg, 0, sizeof(osarg));
	osarg.num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;

	uuid_to_octets(osarg.uuid, destination);
	osarg.clnt_login = connection_method;

	rc = tee_client_open_session(ctx->ctx, &osarg, params);
	if (rc) {
		eorig = TEEC_ORIGIN_COMMS;
		res = osarg.ret;
		goto out_free_tmprefs;
	}
	res = osarg.ret;
	eorig = osarg.ret_origin;
	if (res == TEEC_SUCCESS) {
		session->ctx = ctx;
		session->session_id = osarg.session;
	}
	_TEEC_PostProcess_Operation(operation, params, shms);

out_free_tmprefs:
	_TEEC_Free_tmprefs(operation, shms);
out:
	if (ret_origin)
		*ret_origin = eorig;
	return res;
}
EXPORT_SYMBOL(TEEC_OpenSession);

void TEEC_CloseSession(struct TEEC_Session *session)
{
	if (!session)
		return;

	tee_client_close_session(session->ctx->ctx, session->session_id);
}
EXPORT_SYMBOL(TEEC_CloseSession);

TEEC_Result TEEC_InvokeCommand(struct TEEC_Session *session, uint32_t cmd_id,
		struct TEEC_Operation *operation, uint32_t *error_origin)
{
	struct tee_ioctl_invoke_arg ivarg;
	struct tee_param params[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	TEEC_Result res;
	uint32_t eorig;
	struct TEEC_SharedMemory shms[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	int rc;

	if (!session) {
		eorig = TEEC_ORIGIN_API;
		res = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	res = _TEEC_PreProcess_Operation(session->ctx, operation, params, shms);
	if (res != TEEC_SUCCESS) {
		eorig = TEEC_ORIGIN_API;
		goto out_free_tmprefs;
	}

	memset(&ivarg, 0, sizeof(ivarg));
	ivarg.num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;
	ivarg.session = session->session_id;
	ivarg.func = cmd_id;

	rc = tee_client_invoke_func(session->ctx->ctx, &ivarg, params);
	if (rc) {
		eorig = TEEC_ORIGIN_COMMS;
		res = ivarg.ret;
		goto out_free_tmprefs;
	}

	res = ivarg.ret;
	eorig = ivarg.ret_origin;
	_TEEC_PostProcess_Operation(operation, params, shms);

out_free_tmprefs:
	_TEEC_Free_tmprefs(operation, shms);
out:
	if (error_origin)
		*error_origin = eorig;
	return res;
}
EXPORT_SYMBOL(TEEC_InvokeCommand);

TEEC_Result TEEC_RegisterSharedMemory(struct TEEC_Context *ctx,
					struct TEEC_SharedMemory *shm)
{
	int id;
	int rc;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	rc = tee_client_register_shm(ctx->ctx,
				     (unsigned long)shm->buffer,
				     shm->size, &id);
	if (rc != 0)
		return TEEC_ERROR_GENERIC;

	shm->pteeshm = tee_shm_get_from_id(ctx->ctx, id);
	if (shm->pteeshm == NULL)
		return TEEC_ERROR_OUT_OF_MEMORY;

	shm->alloc = 0;
	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(TEEC_RegisterSharedMemory);

TEEC_Result TEEC_AllocateSharedMemory(struct TEEC_Context *ctx,
					struct TEEC_SharedMemory *shm)
{
	int id, rc;
	size_t s;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	s = shm->size;
	if (!s)
		s = 8;

	shm->buffer = kmalloc(s, GFP_KERNEL);
	if (shm->buffer == NULL)
		return TEEC_ERROR_OUT_OF_MEMORY;

	rc = tee_client_register_shm(ctx->ctx,
				     (unsigned long)shm->buffer,
				     s, &id);
	if (rc != 0) {
		kfree(shm->buffer);
		return TEEC_ERROR_GENERIC;
	}

	shm->pteeshm = tee_shm_get_from_id(ctx->ctx, id);
	if (shm->pteeshm == NULL) {
		kfree(shm->buffer);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	shm->alloc = 1;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(TEEC_AllocateSharedMemory);

void TEEC_ReleaseSharedMemory(struct TEEC_SharedMemory *shm)
{
	if (!shm || !shm->pteeshm)
		return;

	tee_client_unregister_shm(shm->pteeshm->ctx,
				  tee_shm_get_id(shm->pteeshm));
	shm->pteeshm = NULL;
	if (shm->alloc)
		kfree(shm->buffer);
	shm->buffer = NULL;
	shm->size = 0;
}
EXPORT_SYMBOL(TEEC_ReleaseSharedMemory);

MODULE_LICENSE("GPL v2");
