/*
 *  Copyright (C) 2014. Mellanox Technologies, Ltd. ALL RIGHTS RESERVED.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License") you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 *    THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 *    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 *    LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 *    FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 *    See the Apache Version 2.0 License for specific language governing
 *    permissions and limitations under the License.
 *
 */

#include "sai_windows.h"
#include "sai.h"
#include "mlnx_sai.h"
#include "assert.h"
#include <limits.h>
#include <math.h>

#undef  __MODULE__
#define __MODULE__ SAI_BUFFER

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;
static mlnx_sai_buffer_resource_limits_t buffer_limits;
#define SAI_SDK_DEFAULT_MC_BUFFER_RESERVED_SIZE 213

typedef enum _sai_buffer_alpha_bounds_t {
    SAI_BUFFER_ALPHA_0 = -8, /* Values <= SAI_BUFFER_ALPHA_0 are interpreted as 0 */

    SAI_BUFFER_ALPHA_1_128 = -7,
    SAI_BUFFER_ALPHA_1_64  = -6,
    SAI_BUFFER_ALPHA_1_32  = -5,
    SAI_BUFFER_ALPHA_1_16  = -4,
    SAI_BUFFER_ALPHA_1_8   = -3,
    SAI_BUFFER_ALPHA_1_4   = -2,
    SAI_BUFFER_ALPHA_1_2   = -1,

    SAI_BUFFER_ALPHA_1  = 0,
    SAI_BUFFER_ALPHA_2  = 1,
    SAI_BUFFER_ALPHA_4  = 2,
    SAI_BUFFER_ALPHA_8  = 3,
    SAI_BUFFER_ALPHA_16 = 4,
    SAI_BUFFER_ALPHA_32 = 5,
    SAI_BUFFER_ALPHA_64 = 6,

    SAI_BUFFER_ALPHA_INFINITY = 7 /* Values >= SAI_BUFFER_ALPHA_INFINITY are interpreted as 'infinity'*/
} sai_buffer_alpha_bounds_t;
typedef struct _mlnx_affect_port_buff_items {
    uint32_t affected_count;
    bool   * pgs;
    bool   * tcs;
    bool   * i_port_buffers;
    bool   * e_port_buffers;
} mlnx_affect_port_buff_items_t;

static bool mlnx_sai_buffer_check_pool_db_consistency(bool expected);
static sai_status_t mlnx_get_sai_pool_data(_In_ sai_object_id_t               sai_pool,
                                           _Out_ mlnx_sai_buffer_pool_attr_t* sai_pool_attr);
static sai_status_t mlnx_sai_buffer_unbind_shared_buffers(_In_ sx_port_log_id_t log_port);
static sai_status_t mlnx_sai_buffer_unbind_reserved_buffers(_In_ sx_port_log_id_t log_port);
static sai_status_t mlnx_sai_buffer_delete_all_buffer_config();
static sai_status_t mlnx_sai_buffer_reset_all_sx_pools();
static sai_status_t mlnx_sai_buffer_delete_all_pools();
static void log_buffer_profile_refs(_In_ mlnx_affect_port_buff_items_t const* refs);
sai_status_t mlnx_create_sai_pool_id(_In_ uint32_t sx_pool_id, _Out_ sai_object_id_t*  sai_pool);
static sai_status_t mlnx_sai_is_buffer_in_use(_In_ sai_object_id_t buffer_profile_id);
static sai_status_t mlnx_sai_collect_buffer_refs(_In_ sai_object_id_t                 sai_buffer_id,
                                                 _In_ uint32_t                        db_port_ind,
                                                 _Out_ mlnx_affect_port_buff_items_t* affected_items);
static sai_status_t mlnx_sai_buffer_configure_reserved_buffers(_In_ sx_port_log_id_t            logical_port,
                                                               _Out_ sx_cos_port_buffer_attr_t* sx_port_reserved_buff_attr_arr,
                                                               _In_ uint32_t                    count);
static sai_status_t mlnx_sai_buffer_configure_shared_buffers(_In_ sx_port_log_id_t                   logical_port,
                                                             _Out_ sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr,
                                                             _In_ uint32_t                           count);
static sai_status_t mlnx_sai_buffer_apply_buffer_change_to_references(_In_ sai_object_id_t sai_buffer_id,
                                                                      _In_ sai_object_id_t prev_pool);
static sai_status_t mlnx_sai_buffer_apply_buffer_to_queue(_In_ uint32_t                           qos_db_port_ind,
                                                          _In_ uint32_t                           qos_ind,
                                                          _In_ mlnx_sai_db_buffer_profile_entry_t buff_db_entry,
                                                          _In_ sai_object_id_t                    prev_pool);
static sai_status_t mlnx_sai_buffer_apply_buffer_to_pg(_In_ uint32_t                           port_ind,
                                                       _In_ uint32_t                           pg_ind,
                                                       _In_ mlnx_sai_db_buffer_profile_entry_t buff_db_entry,
                                                       _In_ sai_object_id_t                    prev_pool);
static sai_status_t mlnx_get_sai_buffer_profile_data(_In_ sai_object_id_t               sai_buffer,
                                                     _Out_ uint32_t                   * out_db_buffer_profile_index,
                                                     _Out_ mlnx_sai_buffer_pool_attr_t* sai_pool_attr);
static sai_status_t mlnx_sai_set_ingress_priority_group_buffer_profile_attr(_In_ const sai_object_key_t      * key,
                                                                            _In_ const sai_attribute_value_t * value,
                                                                            void                             * arg);
static sai_status_t mlnx_sai_get_ingress_priority_group_buffer_profile_attr(_In_ const sai_object_key_t   * key,
                                                                            _Inout_ sai_attribute_value_t * value,
                                                                            _In_ uint32_t                   attr_index,
                                                                            _Inout_ vendor_cache_t        * cache,
                                                                            void                          * arg);
static sai_status_t mlnx_create_sai_buffer_profile_id(_In_ uint32_t          db_buffer_profile_ind,
                                                      _Out_ sai_object_id_t* sai_buffer_profile);
static sai_status_t mlnx_sai_create_buffer_pool(_Out_ sai_object_id_t     * pool_id,
                                                _In_ uint32_t               attr_count,
                                                _In_ const sai_attribute_t *attr_list);
static sai_status_t mlnx_sai_remove_buffer_pool(_In_ sai_object_id_t pool_id);
static sai_status_t mlnx_sai_set_buffer_pool_attr(_In_ sai_object_id_t pool_id, _In_ const sai_attribute_t *    attr);
static sai_status_t mlnx_sai_get_buffer_pool_attr(_In_ sai_object_id_t      pool_id,
                                                  _In_ uint32_t             attr_count,
                                                  _Inout_ sai_attribute_t * attr_list);
static sai_status_t mlnx_sai_create_buffer_profile(_Out_ sai_object_id_t     * buffer_profile_id,
                                                   _In_ uint32_t               attr_count,
                                                   _In_ const sai_attribute_t *attr_list);
static sai_status_t mlnx_sai_remove_buffer_profile(_In_ sai_object_id_t buffer_profile_id);
static sai_status_t mlnx_sai_set_buffer_profile_attr(_In_ sai_object_id_t        buffer_profile_id,
                                                     _In_ const sai_attribute_t *attr);
static sai_status_t mlnx_sai_get_buffer_profile_attr(_In_ sai_object_id_t     buffer_profile_id,
                                                     _In_ uint32_t            attr_count,
                                                     _Inout_ sai_attribute_t *attr_list);
static sai_status_t mlnx_sai_get_pool_shared_size_attr(_In_ const sai_object_key_t   * key,
                                                       _Inout_ sai_attribute_value_t * value,
                                                       _In_ uint32_t                   attr_index,
                                                       _Inout_ vendor_cache_t        * cache,
                                                       void                          * arg);
static sai_status_t mlnx_sai_get_pool_type_attr(_In_ const sai_object_key_t   * key,
                                                _Inout_ sai_attribute_value_t * value,
                                                _In_ uint32_t                   attr_index,
                                                _Inout_ vendor_cache_t        * cache,
                                                void                          * arg);
static sai_status_t mlnx_sai_get_pool_size_attr(_In_ const sai_object_key_t   * key,
                                                _Inout_ sai_attribute_value_t * value,
                                                _In_ uint32_t                   attr_index,
                                                _Inout_ vendor_cache_t        * cache,
                                                void                          * arg);
static sai_status_t mlnx_sai_set_pool_size_attr(_In_ const sai_object_key_t      * key,
                                                _In_ const sai_attribute_value_t * value,
                                                void                             * arg);
static sai_status_t mlnx_sai_get_pool_th_mode_attr(_In_ const sai_object_key_t   * key,
                                                   _Inout_ sai_attribute_value_t * value,
                                                   _In_ uint32_t                   attr_index,
                                                   _Inout_ vendor_cache_t        * cache,
                                                   void                          * arg);
static sai_status_t mlnx_sai_get_buffer_profile_pool_id_attr(_In_ const sai_object_key_t   * key,
                                                             _Inout_ sai_attribute_value_t * value,
                                                             _In_ uint32_t                   attr_index,
                                                             _Inout_ vendor_cache_t        * cache,
                                                             void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_pool_id_attr(_In_ const sai_object_key_t      * key,
                                                             _In_ const sai_attribute_value_t * value,
                                                             void                             * arg);
static sai_status_t mlnx_sai_get_buffer_profile_size_attr(_In_ const sai_object_key_t   * key,
                                                          _Inout_ sai_attribute_value_t * value,
                                                          _In_ uint32_t                   attr_index,
                                                          _Inout_ vendor_cache_t        * cache,
                                                          void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_size_attr(_In_ const sai_object_key_t      * key,
                                                          _In_ const sai_attribute_value_t * value,
                                                          void                             * arg);
static sai_status_t mlnx_sai_get_buffer_profile_dynamic_th_attr(_In_ const sai_object_key_t   * key,
                                                                _Inout_ sai_attribute_value_t * value,
                                                                _In_ uint32_t                   attr_index,
                                                                _Inout_ vendor_cache_t        * cache,
                                                                void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_dynamic_th_attr(_In_ const sai_object_key_t      * key,
                                                                _In_ const sai_attribute_value_t * value,
                                                                void                             * arg);
static sai_status_t mlnx_sai_get_buffer_profile_static_th_attr(_In_ const sai_object_key_t   * key,
                                                               _Inout_ sai_attribute_value_t * value,
                                                               _In_ uint32_t                   attr_index,
                                                               _Inout_ vendor_cache_t        * cache,
                                                               void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_static_th_attr(_In_ const sai_object_key_t      * key,
                                                               _In_ const sai_attribute_value_t * value,
                                                               void                             * arg);
static sai_status_t mlnx_sai_get_buffer_profile_xoff_attr(_In_ const sai_object_key_t   * key,
                                                          _Inout_ sai_attribute_value_t * value,
                                                          _In_ uint32_t                   attr_index,
                                                          _Inout_ vendor_cache_t        * cache,
                                                          void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_xoff_attr(_In_ const sai_object_key_t      * key,
                                                          _In_ const sai_attribute_value_t * value,
                                                          void                             * arg);
static sai_status_t mlnx_sai_get_buffer_profile_xon_attr(_In_ const sai_object_key_t   * key,
                                                         _Inout_ sai_attribute_value_t * value,
                                                         _In_ uint32_t                   attr_index,
                                                         _Inout_ vendor_cache_t        * cache,
                                                         void                          * arg);
static sai_status_t mlnx_sai_set_buffer_profile_xon_attr(_In_ const sai_object_key_t      * key,
                                                         _In_ const sai_attribute_value_t * value,
                                                         void                             * arg);
sai_status_t mlnx_buffer_convert_alpha_sai_to_sx(_In_ sai_int8_t sai_alpha, _Out_ sx_cos_port_buff_alpha_e* sx_alpha);
static const sai_attribute_entry_t        pg_attribs[] = {
    { SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE, false, false, true, true,
      "PG buffer profile", SAI_ATTR_VAL_TYPE_OID },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, false,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};
static const sai_vendor_attribute_entry_t pg_vendor_attribs[] = {
    {
        SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE,
        { false, false, true, true },
        { false, false, true, true },
        mlnx_sai_get_ingress_priority_group_buffer_profile_attr, NULL,
        mlnx_sai_set_ingress_priority_group_buffer_profile_attr, NULL
    }
};
static const sai_attribute_entry_t        pool_attribs[] = {
    { SAI_BUFFER_POOL_ATTR_SHARED_SIZE, false, false, false, true,
      "Pool shared size", SAI_ATTR_VAL_TYPE_U32 },

    { SAI_BUFFER_POOL_ATTR_TYPE, true, true, false, true,
      "Pool type", SAI_ATTR_VAL_TYPE_S32 },

    { SAI_BUFFER_POOL_ATTR_SIZE, true, true, true, true,
      "Pool size", SAI_ATTR_VAL_TYPE_U32 },

    { SAI_BUFFER_POOL_ATTR_TH_MODE, false, true, false, true,
      "Pool treshold mode", SAI_ATTR_VAL_TYPE_S32 },

    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, false,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};
static const sai_vendor_attribute_entry_t pool_vendor_attribs[] = {
    {
        SAI_BUFFER_POOL_ATTR_SHARED_SIZE,
        { false, false, false, true },
        { false, false, false, true },
        mlnx_sai_get_pool_shared_size_attr, NULL,
        NULL, NULL
    },
    {
        SAI_BUFFER_POOL_ATTR_TYPE,
        { true, false, false, true },
        { true, false, false, true },
        mlnx_sai_get_pool_type_attr, NULL,
        NULL, NULL
    },
    {
        SAI_BUFFER_POOL_ATTR_SIZE,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_pool_size_attr, NULL,
        mlnx_sai_set_pool_size_attr, NULL
    },
    {
        SAI_BUFFER_POOL_ATTR_TH_MODE,
        { true, false, false, true },
        { true, false, false, true },
        mlnx_sai_get_pool_th_mode_attr, NULL,
        NULL, NULL
    }
};
static const sai_attribute_entry_t        buffer_profile_attribs[] = {
    { SAI_BUFFER_PROFILE_ATTR_POOL_ID, true, true, true, true,
      "Buffer Profile's pool object id", SAI_ATTR_VAL_TYPE_OID },
    { SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE, true, true, true, true,
      "Buffer Profile size", SAI_ATTR_VAL_TYPE_U32 },
    { SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH, false, true, true, true,
      "Buffer Profile shared dynamic treshold", SAI_ATTR_VAL_TYPE_S8 },
    { SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH, false, true, true, true,
      "Buffer profile shared static treshold", SAI_ATTR_VAL_TYPE_U32 },
    { SAI_BUFFER_PROFILE_ATTR_XOFF_TH, false, true, true, true,
      "Buffer Profile XOFF", SAI_ATTR_VAL_TYPE_U32 },
    { SAI_BUFFER_PROFILE_ATTR_XON_TH, false, true, true, true,
      "Buffer Profile XON", SAI_ATTR_VAL_TYPE_U32 },
    { SAI_BUFFER_PROFILE_ATTR_TH_MODE, false, true, true, true,
      "Buffer Profile threshold mode", SAI_ATTR_VAL_TYPE_S32 },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, false,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};
static const sai_vendor_attribute_entry_t buffer_profile_vendor_attribs[] = {
    {
        SAI_BUFFER_PROFILE_ATTR_POOL_ID,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_pool_id_attr, NULL,
        mlnx_sai_set_buffer_profile_pool_id_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_size_attr, NULL,
        mlnx_sai_set_buffer_profile_size_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_dynamic_th_attr, NULL,
        mlnx_sai_set_buffer_profile_dynamic_th_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_static_th_attr, NULL,
        mlnx_sai_set_buffer_profile_static_th_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_XOFF_TH,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_xoff_attr, NULL,
        mlnx_sai_set_buffer_profile_xoff_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_XON_TH,
        { true, false, true, true },
        { true, false, true, true },
        mlnx_sai_get_buffer_profile_xon_attr, NULL,
        mlnx_sai_set_buffer_profile_xon_attr, NULL
    },
    {
        SAI_BUFFER_PROFILE_ATTR_TH_MODE,
        { false, false, false, false },
        { false, false, false, false },
        NULL, NULL,
        NULL, NULL
    },
};


void init_buffer_resource_limits()
{
    buffer_limits.num_ingress_pools   = g_resource_limits.shared_buff_num_ingress_pools;
    buffer_limits.num_egress_pools    = g_resource_limits.shared_buff_num_egress_pools;
    buffer_limits.num_total_pools     = g_resource_limits.shared_buff_total_num_pools;
    buffer_limits.num_port_queue_buff = g_resource_limits.shared_buff_num_port_egress_buff - 1;   /* we need 1 less without CTRL TC */
    buffer_limits.num_port_pg_buff    = g_resource_limits.shared_buff_num_port_ingress_buff - 1;   /* g_resource_limits is including CTRL PG#9. We Need 1 less, without CTRL PG */
    buffer_limits.unit_size           = g_resource_limits.shared_buff_buffer_unit_size;

    buffer_limits.max_buffers_per_port =
        (g_resource_limits.shared_buff_num_ingress_pools +
         g_resource_limits.shared_buff_num_egress_pools +
         g_resource_limits.shared_buff_num_port_egress_buff +
         g_resource_limits.shared_buff_num_port_ingress_buff);

    printf("%s[\n", __FUNCTION__);
    printf("num_ingress_pools:%d\n", buffer_limits.num_ingress_pools);
    printf("num_egress_pools:%d\n", buffer_limits.num_egress_pools);
    printf("num_total_pools:%d\n", buffer_limits.num_total_pools);
    printf("num_port_queue_buff:%d\n", buffer_limits.num_port_queue_buff);
    printf("num_port_pg_buff:%d\n", buffer_limits.num_port_pg_buff);
    printf("unit_size:%d\n", buffer_limits.unit_size);
    printf("max_buffers_per_port:%d\n", buffer_limits.max_buffers_per_port);
    printf("%s]\n", __FUNCTION__);
}

const mlnx_sai_buffer_resource_limits_t* mlnx_sai_get_buffer_resource_limits()
{
    return &buffer_limits;
}

static uint32_t mlnx_sai_get_buffer_profile_number()
{
    return (1 + (MAX_PORTS * buffer_limits.max_buffers_per_port));
}

sai_status_t mlnx_sai_get_port_buffer_index_array(uint32_t                       db_port_ind,
                                                  port_buffer_index_array_type_t buff_type,
                                                  uint32_t                    ** index_arr)
{
    SX_LOG_ENTER();
    uint32_t array_location_offset = 0;

    if (NULL == index_arr) {
        SX_LOG_ERR("NULL index_arr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (db_port_ind >= MAX_PORTS) {
        SX_LOG_ERR("db_port_ind out of bounds\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    switch (buff_type) {
    case PORT_BUFF_TYPE_INGRESS:
        array_location_offset = db_port_ind * buffer_limits.num_ingress_pools;
        break;

    case PORT_BUFF_TYPE_EGRESS:
        array_location_offset =
            buffer_limits.num_ingress_pools * MAX_PORTS +
            db_port_ind * buffer_limits.num_egress_pools;
        break;

    case PORT_BUFF_TYPE_PG:
        array_location_offset =
            buffer_limits.num_ingress_pools * MAX_PORTS +
            buffer_limits.num_egress_pools * MAX_PORTS +
            db_port_ind * buffer_limits.num_port_pg_buff;
        break;

    default:
        SX_LOG_ERR("Invalid buffer type:%d\n", buff_type);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    *index_arr = g_sai_buffer_db_ptr->port_buffer_data + array_location_offset;
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static bool* mlnx_sai_get_port_buffer_pool_allocation_array()
{
    return g_sai_buffer_db_ptr->pool_allocation;
}

_Success_(return == SAI_STATUS_SUCCESS)
static sai_status_t mlnx_sai_buffer_get_e_cpu_pool_state(_Out_ bool *state)
{
    if (NULL == state) {
        SX_LOG_ERR("NULL state\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    *state = mlnx_sai_get_port_buffer_pool_allocation_array()[BUFFER_DB_POOL_E_CPU_POOL_IND];
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_set_e_cpu_pool_state(_In_ bool state)
{
    if (!mlnx_sai_buffer_check_pool_db_consistency(!state)) {
        return SAI_STATUS_FAILURE;
    }
    mlnx_sai_get_port_buffer_pool_allocation_array()[BUFFER_DB_POOL_E_CPU_POOL_IND] = state;
    SX_LOG_NTC("marked e_cpu slot with state:%d\n", state);
    return SAI_STATUS_SUCCESS;
}

static uint32_t* mlnx_sai_get_e_cpu_pool_size_slot()
{
    return g_sai_buffer_db_ptr->e_cpu_pool_startup_size;
}

/*
 *   Save the initial size of the pool#7 (e_cpu pool).
 *   This pool is never physically deleted.
 *   When/if user creates/deletes this pool we actually mark it in buffer DB as 'created/deleted',
 *   by calling mlnx_sai_buffer_set_e_cpu_pool_state().
 *   In case of deletion we set the size of this pool back to the value SDK initializes it with.
 */
static sai_status_t mlnx_sai_buffer_save_e_cpu_pool_initial_size()
{
    sx_cos_pool_attr_t sx_pool_attr;
    sx_status_t        sx_status;

    SX_LOG_ENTER();
    if (SX_STATUS_SUCCESS !=
        (sx_status = sx_api_cos_shared_buff_pool_get(gh_sdk, EGRESS_CPU_PORT_SX_POOL_ID, &sx_pool_attr))) {
        SX_LOG_ERR("Cannot get pool:%d data. reason:%d, %s. line:%d\n",
                   EGRESS_CPU_PORT_SX_POOL_ID,
                   sx_status,
                   SX_STATUS_MSG(sx_status),
                   __LINE__);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    *(mlnx_sai_get_e_cpu_pool_size_slot()) = sx_pool_attr.pool_size;
    SX_LOG_NTC("Saved size of e_cpu pool:%d\n", *(mlnx_sai_get_e_cpu_pool_size_slot()));
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_get_e_cpu_pool_initial_size(_Out_ uint32_t* size_cells)
{
    *size_cells = *(mlnx_sai_get_e_cpu_pool_size_slot());
    SX_LOG_NTC("Obtained size of e_cpu pool:%d\n", *size_cells);
    return SAI_STATUS_SUCCESS;
}

static bool mlnx_sai_buffer_get_pool_create_triggered_flag()
{
    return mlnx_sai_get_port_buffer_pool_allocation_array()[BUFFER_DB_POOL_FLAG_ARRAY_SIZE];
}

static sai_status_t mlnx_sai_buffer_set_pool_raise_triggered_flag()
{
    if (mlnx_sai_get_port_buffer_pool_allocation_array()[BUFFER_DB_POOL_FLAG_ARRAY_SIZE]) {
        SX_LOG_ERR("Double setting the initial pool creation flag\n");
        return SAI_STATUS_FAILURE;
    }
    mlnx_sai_get_port_buffer_pool_allocation_array()[BUFFER_DB_POOL_FLAG_ARRAY_SIZE] = true;
    return SAI_STATUS_SUCCESS;
}

static void mlnx_sai_buffer_log_pool_db()
{
    sai_status_t sai_status;
    bool         state;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_get_e_cpu_pool_state(&state))) {
        SX_LOG_EXIT();
        return;
    }
    SX_LOG_DBG("e_cpu pool state == %d\n", state);
    SX_LOG_DBG("pool creation triggered flag:%d\n", mlnx_sai_buffer_get_pool_create_triggered_flag());
    SX_LOG_EXIT();
}

static sai_status_t mlnx_sai_buffer_pool_db_alloc_e_cpu_port_slot()
{
    sai_status_t sai_status;

    SX_LOG_ENTER();
    mlnx_sai_buffer_log_pool_db();
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_set_e_cpu_pool_state(true))) {
        /* SX SDK returns SX_STATUS_NO_RESOURCES when no pools are available, which is mapped to this value */
        SX_LOG_ERR("e_cpu pool DB slot in use\n");
        return SAI_STATUS_INSUFFICIENT_RESOURCES;
    }
    SX_LOG_DBG("allocated e_cpu pool\n");
    mlnx_sai_buffer_log_pool_db();
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static bool mlnx_sai_buffer_check_pool_db_consistency(bool expected)
{
    bool pool_db_state;

    if (SAI_STATUS_SUCCESS != mlnx_sai_buffer_get_e_cpu_pool_state(&pool_db_state)) {
        return false;
    }
    if (expected != pool_db_state) {
        SX_LOG_ERR("pool_id:%d db state:%d not equal to expected:%d\n",
                   EGRESS_CPU_PORT_SX_POOL_ID,
                   pool_db_state,
                   expected);
        return false;
    }
    return true;
}

#define max_value(x, y) ((x > y) ? x : y)

uint32_t mlnx_cells_to_bytes(uint32_t cells)
{
    return (cells * g_resource_limits.shared_buff_buffer_unit_size);
}

buffer_units_t bytes_to_mlnx_cells(uint32_t bytes)
{
    return ((buffer_units_t)ceil(((double)bytes) / (double)g_resource_limits.shared_buff_buffer_unit_size));
}

static bool alloc_affected_items(_Out_ mlnx_affect_port_buff_items_t* affected_items)
{
    bool ret = false;

    memset(affected_items, 0, sizeof(mlnx_affect_port_buff_items_t));
    if (NULL == (affected_items->pgs = calloc(buffer_limits.num_port_pg_buff, sizeof(bool)))) {
        goto exit;
    }
    if (NULL == (affected_items->tcs = calloc(buffer_limits.num_port_queue_buff, sizeof(bool)))) {
        goto exit;
    }
    if (NULL == (affected_items->i_port_buffers = calloc(buffer_limits.num_ingress_pools, sizeof(bool)))) {
        goto exit;
    }
    if (NULL == (affected_items->e_port_buffers = calloc(buffer_limits.num_egress_pools, sizeof(bool)))) {
        goto exit;
    }
    ret = true;
exit:
    if (!ret) {
        if (affected_items->pgs) {
            free(affected_items->pgs);
        }
        if (affected_items->tcs) {
            free(affected_items->tcs);
        }
        if (affected_items->i_port_buffers) {
            free(affected_items->i_port_buffers);
        }
        if (affected_items->e_port_buffers) {
            free(affected_items->e_port_buffers);
        }
    }
    return ret;
}

static void reset_affected_items(_Out_ mlnx_affect_port_buff_items_t* affected_items)
{
    affected_items->affected_count = 0;
    memset(affected_items->pgs, 0, buffer_limits.num_port_pg_buff * sizeof(bool));
    memset(affected_items->tcs, 0, buffer_limits.num_port_queue_buff * sizeof(bool));
    memset(affected_items->i_port_buffers, 0, buffer_limits.num_ingress_pools * sizeof(bool));
    memset(affected_items->e_port_buffers, 0, buffer_limits.num_egress_pools * sizeof(bool));
}

static void free_affected_items(_Out_ mlnx_affect_port_buff_items_t* affected_items)
{
    free(affected_items->pgs);
    affected_items->pgs = NULL;
    free(affected_items->tcs);
    affected_items->tcs = NULL;
    free(affected_items->i_port_buffers);
    affected_items->i_port_buffers = NULL;
    free(affected_items->e_port_buffers);
    affected_items->e_port_buffers = NULL;
}

sai_status_t mlnx_sai_buffer_log_set(_In_ sx_verbosity_level_t level)
{
    LOG_VAR_NAME(__MODULE__) = level;
    if (gh_sdk) {
        return sdk_to_sai(sx_api_cos_log_verbosity_level_set(gh_sdk, SX_LOG_VERBOSITY_BOTH, level, level));
    } else {
        return SAI_STATUS_SUCCESS;
    }
}

static void pool_key_to_str(_In_ sai_object_id_t sai_pool, _Out_ char              *key_str)
{
    uint32_t sx_pool_id = 0;

    if (NULL == key_str) {
        SX_LOG_ERR("NULL key_str\n");
        return;
    }

    if (SAI_STATUS_SUCCESS != mlnx_object_to_type(sai_pool, SAI_OBJECT_TYPE_BUFFER_POOL, &sx_pool_id, NULL)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "Invalid sai pool key");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "pool id:%u", sx_pool_id);
    }
}

static void pg_key_to_str(_In_ sai_object_id_t sai_port_pg, _Out_ char              *key_str)
{
    uint32_t port_db_index = 0;
    uint8_t  extended_data[EXTENDED_DATA_SIZE];

    memset(extended_data, 0, sizeof(extended_data));

    if (NULL == key_str) {
        SX_LOG_ERR("NULL key_str\n");
        return;
    }

    if (SAI_STATUS_SUCCESS !=
        mlnx_object_to_type(sai_port_pg, SAI_OBJECT_TYPE_PRIORITY_GROUP, &port_db_index, extended_data)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "Invalid port PG key");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "port:%u, PG index:%u", port_db_index, extended_data[0]);
    }
}

static void buffer_profile_key_to_str(_In_ sai_object_id_t sai_buffer_profile, _Out_ char              *key_str)
{
    uint32_t buffer_db_index = 0;

    if (SAI_STATUS_SUCCESS !=
        mlnx_object_to_type(sai_buffer_profile, SAI_OBJECT_TYPE_BUFFER_PROFILE, &buffer_db_index, NULL)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "Invalid buffer profile");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "profile index:0x%x", buffer_db_index);
    }
}

static void log_buffer_profile_refs(_In_ mlnx_affect_port_buff_items_t const* refs)
{
    uint32_t ind;

    SX_LOG_ENTER();

    for (ind = 0; ind < buffer_limits.num_port_pg_buff; ind++) {
        if (refs->pgs[ind]) {
            SX_LOG_DBG("pg[%d]\n", ind);
        }
    }
    for (ind = 0; ind < buffer_limits.num_port_queue_buff; ind++) {
        if (refs->tcs[ind]) {
            SX_LOG_DBG("tc[%d]\n", ind);
        }
    }
    for (ind = 0; ind < buffer_limits.num_ingress_pools; ind++) {
        if (refs->i_port_buffers[ind]) {
            SX_LOG_DBG("i_port_buffers[%d]\n", ind);
        }
    }
    for (ind = 0; ind < buffer_limits.num_egress_pools; ind++) {
        if (refs->e_port_buffers[ind]) {
            SX_LOG_DBG("e_port_buffers[%d]\n", ind);
        }
    }
    SX_LOG_EXIT();
}

static void log_sx_pool_mode(_In_ sx_cos_buffer_max_mode_e mode)
{
    if (SX_COS_BUFFER_MAX_MODE_STATIC_E == mode) {
        SX_LOG_DBG("SX_COS_BUFFER_MAX_MODE_STATIC_E\n");
    }

    if (SX_COS_BUFFER_MAX_MODE_DYNAMIC_E == mode) {
        SX_LOG_DBG("SX_COS_BUFFER_MAX_MODE_DYNAMIC_E\n");
    }
}

static void log_sx_shared_max_size(sx_cos_buffer_max_t max)
{
    SX_LOG_ENTER();

    log_sx_pool_mode(max.mode);

    if (SX_COS_BUFFER_MAX_MODE_STATIC_E == max.mode) {
        SX_LOG_DBG("size:%d\n", max.max.size);
    } else if (SX_COS_BUFFER_MAX_MODE_DYNAMIC_E == max.mode) {
        SX_LOG_DBG("alpha:%d\n", max.max.alpha);
    } else {
        SX_LOG_WRN("Unknown max.mode:%d\n", max.mode);
    }

    SX_LOG_EXIT();
}

static sai_status_t log_sai_shared_max_size(mlnx_sai_shared_max_size_t shared_max_size)
{
    char       * str;
    sai_status_t sai_status = SAI_STATUS_SUCCESS;

    SX_LOG_ENTER();

    switch (shared_max_size.mode) {
    case SAI_BUFFER_THRESHOLD_MODE_STATIC:
        str = "SAI_BUFFER_THRESHOLD_MODE_STATIC";
        SX_LOG_DBG("mode:%s\n", str);
        SX_LOG_DBG("static_th:%u\n", shared_max_size.max.static_th);
        break;

    case SAI_BUFFER_THRESHOLD_MODE_DYNAMIC:
        str = "SAI_BUFFER_THRESHOLD_MODE_DYNAMIC";
        SX_LOG_DBG("mode:%s\n", str);
        SX_LOG_DBG("alpha:%d\n", shared_max_size.max.alpha);

        break;

    default:
        SX_LOG_ERR("Invalid shared max size mode specified:%d", shared_max_size.mode);
        SX_LOG_DBG("static_th:%u\n", shared_max_size.max.static_th);
        sai_status = SAI_STATUS_INVALID_PARAMETER;
    }

    SX_LOG_EXIT();
    return sai_status;
}

void log_sx_port_buffers(uint32_t                   port_ind,
                         uint32_t                   sx_port_buffer_attr_cnt,
                         sx_cos_port_buffer_attr_t* sx_port_buff_attr)
{
    uint32_t buff_ind;

    SX_LOG_ENTER();
    SX_LOG_DBG("port_db[%d].logical:%d, sx buffer count:%d.\n",
               port_ind, g_sai_db_ptr->ports_db[port_ind].logical, sx_port_buffer_attr_cnt);

    for (buff_ind = 0; buff_ind < sx_port_buffer_attr_cnt; buff_ind++) {
        SX_LOG_DBG("sx_cos_port_buffer_attr_t:%d[\n", buff_ind);
        switch (sx_port_buff_attr[buff_ind].type) {
        case SX_COS_INGRESS_PORT_ATTR_E:

            SX_LOG_DBG("type:SX_COS_INGRESS_PORT_ATTR_E\n");
            SX_LOG_DBG("size:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_buff_attr.size);
            SX_LOG_DBG("pool_id:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_buff_attr.pool_id);
            break;

        case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
            SX_LOG_DBG("type:SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E\n");
            SX_LOG_DBG("size:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.size);
            SX_LOG_DBG("pg:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.pg);
            SX_LOG_DBG("is_lossy:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.is_lossy);
            SX_LOG_DBG("xon:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.xon);
            SX_LOG_DBG("xoff:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.xoff);
            SX_LOG_DBG("pool_id:%d\n", sx_port_buff_attr[buff_ind].attr.ingress_port_pg_buff_attr.pool_id);
            break;

        case SX_COS_EGRESS_PORT_ATTR_E:
            SX_LOG_DBG("type:SX_COS_EGRESS_PORT_ATTR_E\n");
            SX_LOG_DBG("size:%d\n", sx_port_buff_attr[buff_ind].attr.egress_port_buff_attr.size);
            SX_LOG_DBG("pool_id:%d\n", sx_port_buff_attr[buff_ind].attr.egress_port_buff_attr.pool_id);
            break;

        case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
            SX_LOG_DBG("type:SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E\n");
            SX_LOG_DBG("pool_id:%d\n", sx_port_buff_attr[buff_ind].attr.egress_port_tc_buff_attr.pool_id);
            SX_LOG_DBG("size:%d\n", sx_port_buff_attr[buff_ind].attr.egress_port_tc_buff_attr.size);
            SX_LOG_DBG("tc:%d\n", sx_port_buff_attr[buff_ind].attr.egress_port_tc_buff_attr.tc);

            break;

        case SX_COS_MULTICAST_ATTR_E:
            SX_LOG_DBG("type:SX_COS_MULTICAST_ATTR_E\n");
            SX_LOG_DBG("pool_id:%d\n", sx_port_buff_attr[buff_ind].attr.multicast_buff_attr.pool_id);
            SX_LOG_DBG("size:%d\n", sx_port_buff_attr[buff_ind].attr.multicast_buff_attr.size);
            SX_LOG_DBG("sp:%d\n", sx_port_buff_attr[buff_ind].attr.multicast_buff_attr.sp);
            break;

        default:
            SX_LOG_ERR("Unknown buff type\n");
        }
        SX_LOG_DBG("sx_cos_port_buffer_attr_t]\n");
    }

    SX_LOG_EXIT();
}

void log_sx_port_shared_buffers(uint32_t                          port_ind,
                                uint32_t                          sx_port_shared_buffer_attr_cnt,
                                sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr)
{
    uint32_t buff_ind;

    SX_LOG_ENTER();
    SX_LOG_DBG("port_db[%d].logical:%d, sx buffer count:%d.\n",
               port_ind, g_sai_db_ptr->ports_db[port_ind].logical, sx_port_shared_buffer_attr_cnt);

    for (buff_ind = 0; buff_ind < sx_port_shared_buffer_attr_cnt; buff_ind++) {
        SX_LOG_DBG("sx_cos_port_shared_buffer_attr_t:%d[\n", buff_ind);
        switch (sx_port_shared_buff_attr[buff_ind].type) {
        case SX_COS_INGRESS_PORT_ATTR_E:
            SX_LOG_DBG("type:SX_COS_INGRESS_PORT_ATTR_E\n");
            log_sx_shared_max_size(sx_port_shared_buff_attr[buff_ind].attr.ingress_port_shared_buff_attr.max);
            SX_LOG_DBG("pool_id:%d\n", sx_port_shared_buff_attr[buff_ind].attr.ingress_port_shared_buff_attr.pool_id);
            break;

        case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
            SX_LOG_DBG("type:SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E\n");
            log_sx_shared_max_size(sx_port_shared_buff_attr[buff_ind].attr.ingress_port_pg_shared_buff_attr.max);
            SX_LOG_DBG("pg:%d\n", sx_port_shared_buff_attr[buff_ind].attr.ingress_port_pg_shared_buff_attr.pg);
            SX_LOG_DBG("pool_id:%d\n",
                       sx_port_shared_buff_attr[buff_ind].attr.ingress_port_pg_shared_buff_attr.pool_id);
            break;

        case SX_COS_EGRESS_PORT_ATTR_E:
            SX_LOG_DBG("type:SX_COS_EGRESS_PORT_ATTR_E\n");
            log_sx_shared_max_size(sx_port_shared_buff_attr[buff_ind].attr.egress_port_shared_buff_attr.max);
            SX_LOG_DBG("pool_id:%d\n", sx_port_shared_buff_attr[buff_ind].attr.egress_port_shared_buff_attr.pool_id);
            break;

        case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
            SX_LOG_DBG("type:SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E\n");
            log_sx_shared_max_size(sx_port_shared_buff_attr[buff_ind].attr.egress_port_tc_shared_buff_attr.max);
            SX_LOG_DBG("tc:%d\n", sx_port_shared_buff_attr[buff_ind].attr.egress_port_tc_shared_buff_attr.tc);
            SX_LOG_DBG("pool_id:%d\n",
                       sx_port_shared_buff_attr[buff_ind].attr.egress_port_tc_shared_buff_attr.pool_id);
            break;

        case SX_COS_MULTICAST_ATTR_E:
            SX_LOG_DBG("type:SX_COS_MULTICAST_ATTR_E\n");
            log_sx_shared_max_size(sx_port_shared_buff_attr[buff_ind].attr.multicast_shared_buff_attr.max);
            SX_LOG_DBG("sp:%d\n", sx_port_shared_buff_attr[buff_ind].attr.multicast_shared_buff_attr.sp);
            SX_LOG_DBG("pool_id:%d\n", sx_port_shared_buff_attr[buff_ind].attr.multicast_shared_buff_attr.pool_id);
            break;

        default:
            SX_LOG_ERR("Unknown buff type\n");
        }
        SX_LOG_DBG("sx_cos_port_shared_buffer_attr_t]\n");
    }

    SX_LOG_EXIT();
}

static void log_sai_pool_mode(_In_ sai_buffer_threshold_mode_t mode)
{
    if (SAI_BUFFER_THRESHOLD_MODE_STATIC == mode) {
        SX_LOG_DBG("SAI_BUFFER_THRESHOLD_MODE_STATIC\n");
    }
    if (SAI_BUFFER_THRESHOLD_MODE_DYNAMIC == mode) {
        SX_LOG_DBG("SAI_BUFFER_THRESHOLD_MODE_DYNAMIC\n");
    }
}

static void log_sai_pool_type(_In_ sai_buffer_pool_type_t pool_type)
{
    if (SAI_BUFFER_POOL_INGRESS == pool_type) {
        SX_LOG_DBG("pool_type:SAI_BUFFER_POOL_INGRESS \n");
    }
    if (SAI_BUFFER_POOL_EGRESS == pool_type) {
        SX_LOG_DBG("pool_type:SAI_BUFFER_POOL_EGRESS\n");
    }
}

static void log_sai_pool_attribs(_In_ mlnx_sai_buffer_pool_attr_t sai_pool_attr)
{
    SX_LOG_ENTER();
    SX_LOG_DBG("sx_pool_id:%d \n", sai_pool_attr.sx_pool_id);
    log_sai_pool_type(sai_pool_attr.pool_type);
    log_sai_pool_mode(sai_pool_attr.pool_mode);
    SX_LOG_DBG("pool_size:%d\n", sai_pool_attr.pool_size);
    SX_LOG_EXIT();
}

sai_status_t log_sai_pool_data(_In_ sai_object_id_t sai_pool)
{
    sai_status_t                sai_status;
    char                        key_str[MAX_KEY_STR_LEN];
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    pool_key_to_str(sai_pool, key_str);
    sai_status = mlnx_get_sai_pool_data(sai_pool, &sai_pool_attr);
    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_EXIT();
        return sai_status;
    }
    log_sai_pool_attribs(sai_pool_attr);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t log_sai_buffer_profile_db_entry_fields(mlnx_sai_db_buffer_profile_entry_t db_buff)
{
    sai_status_t sai_status;
    char         key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    SX_LOG_DBG("is_valid:%d\n", db_buff.is_valid);
    SX_LOG_DBG("SAI pool:0x%" PRIx64 "\n", db_buff.sai_pool);
    pool_key_to_str(db_buff.sai_pool, key_str);
    SX_LOG_DBG("SAI pool data:%s\n", key_str);
    SX_LOG_DBG("reserved_size:%d\n", db_buff.reserved_size);

    sai_status = log_sai_shared_max_size(db_buff.shared_max);

    SX_LOG_DBG("xon:%d\n", db_buff.xon);
    SX_LOG_DBG("xoff:%d\n", db_buff.xoff);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t log_sai_buffer_profile_db_entry(_In_ uint32_t db_buffer_profile_index)
{
    sai_status_t sai_status = SAI_STATUS_SUCCESS;

    SX_LOG_ENTER();
    if (db_buffer_profile_index >= mlnx_sai_get_buffer_profile_number()) {
        SX_LOG_ERR("Invalid db_buffer_profile_index:%d\n", db_buffer_profile_index);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    mlnx_sai_db_buffer_profile_entry_t* db_buff = &g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index];
    SX_LOG_DBG("db_buffer_profile_index:0x%X\n", db_buffer_profile_index);
    sai_status = log_sai_buffer_profile_db_entry_fields(*db_buff);

    SX_LOG_EXIT();
    return sai_status;
}

static bool is_valid_buffer_profile_db_entry(_In_ uint32_t db_buffer_profile_index)
{
    bool ret;

    SX_LOG_ENTER();
    SX_LOG_DBG("db_buffer_profile_index:0x%X \n", db_buffer_profile_index);
    if (mlnx_sai_get_buffer_profile_number() <= db_buffer_profile_index) {
        SX_LOG_ERR("buffer profile index exeeds range:0x%X \n", mlnx_sai_get_buffer_profile_number());
        SX_LOG_EXIT();
        ret = false;
        goto exit;
    }
    if (!g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].is_valid) {
        SX_LOG_ERR("buffer profile at index :0x%X is marked as unused\n", db_buffer_profile_index);
        ret = false;
        goto exit;
    }
    ret = true;
exit:
    SX_LOG_EXIT();
    return ret;
}
static sai_status_t is_valid_buffer_profile(_In_ sai_object_id_t sai_buffer)
{
    uint32_t     db_buffer_profile_index;
    sai_status_t sai_status;

    SX_LOG_ENTER();
    SX_LOG_DBG("Input sai buffer:0x%" PRIx64 ", \n", sai_buffer);
    if (SAI_NULL_OBJECT_ID == sai_buffer) {
        SX_LOG_EXIT();
        return SAI_STATUS_SUCCESS;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(sai_buffer, SAI_OBJECT_TYPE_BUFFER_PROFILE, &db_buffer_profile_index, NULL))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (!is_valid_buffer_profile_db_entry(db_buffer_profile_index)) {
        SX_LOG_ERR("Input sai buffer:0x%" PRIx64 ", db_buffer_profile_index:0x%X, refers to invalid entry\n",
                   sai_buffer,
                   db_buffer_profile_index);
        sai_status = SAI_STATUS_FAILURE;
    }
    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t get_buffer_profile_db_index(_In_ sai_object_id_t oid, _Out_ uint32_t* db_index)
{
    sai_status_t sai_status;
    uint32_t     input_db_buffer_profile_index;
    char         key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();
    if (NULL == db_index) {
        SX_LOG_ERR("NULL db_index\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_NULL_OBJECT_ID == oid) {
        /* point to sentinel entry in buffer_db */
        SX_LOG_DBG("Input buffer profile is NULL\n");
        *db_index = SENTINEL_BUFFER_DB_ENTRY_INDEX;
        return SAI_STATUS_SUCCESS;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = is_valid_buffer_profile(oid))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    buffer_profile_key_to_str(oid, key_str);
    SX_LOG_DBG("SAI buffer profile id:%s\n", key_str);
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(oid, SAI_OBJECT_TYPE_BUFFER_PROFILE, &input_db_buffer_profile_index, NULL))) {
        SX_LOG_ERR("Failed to obtain input buffer profile's db index\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SENTINEL_BUFFER_DB_ENTRY_INDEX == input_db_buffer_profile_index) {
        SX_LOG_ERR("Invalid Buffer profile db reference. Make sure buffer configuration was initialized.\n");
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_DBG("Input buffer profile:0x%" PRIx64 ", db index:%d\n", oid, input_db_buffer_profile_index);
    *db_index = input_db_buffer_profile_index;
    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t log_sai_buffer_profile(_In_ sai_object_id_t sai_buffer_profile)
{
    sai_status_t sai_status;
    uint32_t     input_db_buffer_profile_index = 0;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS !=
        (sai_status = get_buffer_profile_db_index(sai_buffer_profile, &input_db_buffer_profile_index))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    sai_status = log_sai_buffer_profile_db_entry(input_db_buffer_profile_index);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t get_pg_data(_In_ sai_object_id_t sai_pg,
                                _Out_ uint32_t     * db_port_index,
                                _Out_ uint32_t     * port_pg_index)
{
    sai_status_t sai_status;
    uint8_t      extended_data[EXTENDED_DATA_SIZE];

    SX_LOG_ENTER();

    if (NULL == db_port_index) {
        SX_LOG_ERR("NULL db_port_index\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == port_pg_index) {
        SX_LOG_ERR("NULL port_pg_index\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(sai_pg, SAI_OBJECT_TYPE_PRIORITY_GROUP, db_port_index, extended_data))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (extended_data[0] >= buffer_limits.num_port_pg_buff) {
        SX_LOG_ERR("Invalid pg index:%d, found for db_port_index:%d\n", extended_data[0], *db_port_index);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (*db_port_index >= MAX_PORTS) {
        SX_LOG_ERR("Invalid db_port_index:%d, for pg_index:%d\n", *db_port_index, extended_data[0]);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    *port_pg_index = extended_data[0];
    SX_LOG_DBG("PG details: db_port_index:%d, pg_index:%d\n", *db_port_index, *port_pg_index);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** buffer profile pointer [sai_object_id_t] */
static sai_status_t mlnx_sai_set_ingress_priority_group_buffer_profile_attr(_In_ const sai_object_key_t      * key,
                                                                            _In_ const sai_attribute_value_t * value,
                                                                            void                             * arg)
{
    sai_status_t                       sai_status;
    uint32_t                           db_port_index                 = 0;
    uint32_t                           input_db_buffer_profile_index = 0;
    char                               key_str[MAX_KEY_STR_LEN];
    uint32_t                           port_pg_ind      = 0;
    uint16_t                           pg_buffer_db_ind = 0;
    mlnx_sai_db_buffer_profile_entry_t buff_db_entry;
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr;
    sai_object_id_t                    sai_pool_0;
    uint32_t                         * port_pg_profile_refs = NULL;
    sai_object_id_t                    prev_pool            = SAI_NULL_OBJECT_ID;

    memset(key_str, 0, sizeof(key_str));
    SX_LOG_ENTER();
    pg_key_to_str(key->object_id, key_str);
    SX_LOG_DBG("Input priority group:%s\n", key_str);
    buffer_profile_key_to_str(value->oid, key_str);
    SX_LOG_DBG("Input buffer profile:%s\n", key_str);
    if (SAI_STATUS_SUCCESS != (sai_status = get_pg_data(key->object_id, &db_port_index, &port_pg_ind))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_pool_id(DEFAULT_INGRESS_SX_POOL_ID, &sai_pool_0))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_get_port_buffer_index_array(db_port_index, PORT_BUFF_TYPE_PG, &port_pg_profile_refs))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_NULL_OBJECT_ID == value->oid) {
        SX_LOG_DBG("Resetting buffer profile reference on port[%d].pg[%d]\n", db_port_index, port_pg_ind);
        memset(&buff_db_entry, 0, sizeof(buff_db_entry));
        buff_db_entry.is_valid = true;
        buff_db_entry.sai_pool = sai_pool_0;
        if (SENTINEL_BUFFER_DB_ENTRY_INDEX == port_pg_profile_refs[port_pg_ind]) {
            /* PG refers to default ingress pool, and we're setting it to profile which also refers to the same default ingress pool */
            prev_pool = SAI_NULL_OBJECT_ID;
        } else {
            assert(g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].is_valid);
            if (g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].sai_pool != sai_pool_0) {
                prev_pool = g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].sai_pool;
            }
        }
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_buffer_apply_buffer_to_pg(db_port_index, port_pg_ind, buff_db_entry, prev_pool))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        port_pg_profile_refs[port_pg_ind] = SENTINEL_BUFFER_DB_ENTRY_INDEX;
    } else {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_get_sai_buffer_profile_data(value->oid, &input_db_buffer_profile_index, &sai_pool_attr))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        buff_db_entry = g_sai_buffer_db_ptr->buffer_profiles[input_db_buffer_profile_index];

        /* buffer profile attached to PG must be INGRESS buffer*/
        if (SAI_BUFFER_POOL_INGRESS != sai_pool_attr.pool_type) {
            pg_key_to_str(key->object_id, key_str);
            SX_LOG_ERR("Buffer profile:0x%" PRIx64 " refers to EGRESS pool:0x%" PRIx64 ", Cannot be set on PG:%s\n",
                       value->oid,
                       g_sai_buffer_db_ptr->buffer_profiles[input_db_buffer_profile_index].sai_pool,
                       key_str);

            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
        pg_buffer_db_ind = port_pg_profile_refs[port_pg_ind];
        SX_LOG_DBG("port_db[%d] pg index:%d, pg->buffer_profile db_ind:0x%X. input buffer profile index:%d\n",
                   db_port_index,
                   port_pg_ind,
                   pg_buffer_db_ind,
                   input_db_buffer_profile_index);

        /* For diagnostic purposes  */
        if (SENTINEL_BUFFER_DB_ENTRY_INDEX == pg_buffer_db_ind) {
            SX_LOG_DBG("replace NULL pg_buffer index with index:0x%X\n", input_db_buffer_profile_index);
        } else {
            SX_LOG_DBG("previous buffer profile db_ind:%d, new buffer profile index:0x%X\n",
                       pg_buffer_db_ind,
                       input_db_buffer_profile_index);
        }
        if (SENTINEL_BUFFER_DB_ENTRY_INDEX == port_pg_profile_refs[port_pg_ind]) {
            /* PG refers to default ingress pool */
            if (sai_pool_0 != buff_db_entry.sai_pool) {
                prev_pool = sai_pool_0;
            }
        } else {
            assert(g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].is_valid);
            if (g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].sai_pool !=
                buff_db_entry.sai_pool) {
                prev_pool = g_sai_buffer_db_ptr->buffer_profiles[port_pg_profile_refs[port_pg_ind]].sai_pool;
            }
        }
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_buffer_apply_buffer_to_pg(db_port_index, port_pg_ind, buff_db_entry, prev_pool))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        port_pg_profile_refs[port_pg_ind] = input_db_buffer_profile_index;
        SX_LOG_DBG("Logging newly set buffer profile\n");
        log_sai_buffer_profile_db_entry(input_db_buffer_profile_index);
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** buffer profile pointer [sai_object_id_t] */
static sai_status_t mlnx_sai_get_ingress_priority_group_buffer_profile_attr(_In_ const sai_object_key_t   * key,
                                                                            _Inout_ sai_attribute_value_t * value,
                                                                            _In_ uint32_t                   attr_index,
                                                                            _Inout_ vendor_cache_t        * cache,
                                                                            void                          * arg)
{
    sai_status_t    sai_status;
    uint32_t        db_port_index = 0;
    sai_object_id_t sai_buffer;
    char            key_str[MAX_KEY_STR_LEN];
    uint32_t        port_pg_ind          = 0;
    uint16_t        pg_buffer_db_ind     = 0;
    uint32_t      * port_pg_profile_refs = NULL;

    SX_LOG_ENTER();
    pg_key_to_str(key->object_id, key_str);
    SX_LOG_DBG("Input priority group:%s\n", key_str);
    if (SAI_STATUS_SUCCESS != (sai_status = get_pg_data(key->object_id, &db_port_index, &port_pg_ind))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_get_port_buffer_index_array(db_port_index, PORT_BUFF_TYPE_PG, &port_pg_profile_refs))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    pg_buffer_db_ind = port_pg_profile_refs[port_pg_ind];
    if (SENTINEL_BUFFER_DB_ENTRY_INDEX == pg_buffer_db_ind) {
        sai_buffer = SAI_NULL_OBJECT_ID;
    } else {
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_buffer_profile_id(pg_buffer_db_ind, &sai_buffer))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    cl_plock_release(&g_sai_db_ptr->p_lock);
    buffer_profile_key_to_str(sai_buffer, key_str);
    SX_LOG_DBG("Created sai buffer profile id:%s\n", key_str);
    value->oid = sai_buffer;
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_get_ingress_priority_group_attr(_In_ sai_object_id_t      ingress_pg_id,
                                                             _In_ uint32_t             attr_count,
                                                             _Inout_ sai_attribute_t * attr_list)
{
    const sai_object_key_t key = { .object_id = ingress_pg_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status;

    SX_LOG_ENTER();
    pg_key_to_str(ingress_pg_id, key_str);
    SX_LOG_DBG("Priority group:%s\n", key_str);
    sai_status = sai_get_attributes(&key, key_str, pg_attribs, pg_vendor_attribs, attr_count, attr_list);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_set_ingress_priority_group_attr(_In_ sai_object_id_t        ingress_pg_id,
                                                             _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .object_id = ingress_pg_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status;

    SX_LOG_ENTER();
    pg_key_to_str(ingress_pg_id, key_str);
    sai_status = sai_set_attribute(&key, key_str, pg_attribs, pg_vendor_attribs, attr);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t convert_sai_pool_type_to_sx_pool_direction(_In_ sai_buffer_pool_type_t              sai_pool_type,
                                                               _Out_ sx_cos_port_buff_pool_direction_e* sx_pool_direction)
{
    SX_LOG_DBG("Input pool type:%d\n", sai_pool_type);
    switch (sai_pool_type) {
    case SAI_BUFFER_POOL_INGRESS:
        *sx_pool_direction = SX_COS_PORT_BUFF_POOL_DIRECTION_INGRESS_E;
        break;

    case SAI_BUFFER_POOL_EGRESS:
        *sx_pool_direction = SX_COS_PORT_BUFF_POOL_DIRECTION_EGRESS_E;
        break;

    default:
        SX_LOG_ERR("Invalid value for sai pool direction specified:%d\n", sai_pool_type);
        return SAI_STATUS_INVALID_PARAMETER;
    }
    return SAI_STATUS_SUCCESS;
}

_Success_(return == SAI_STATUS_SUCCESS)
static sai_status_t convert_sai_pool_mode_to_sx_pool_mode(_In_ sai_buffer_threshold_mode_t sai_pool_mode,
                                                          _Out_ sx_cos_buffer_max_mode_e * sx_pool_mode)
{
    if (NULL == sx_pool_mode) {
        SX_LOG_ERR("NULL sx_pool_mode\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    switch (sai_pool_mode) {
    case SAI_BUFFER_THRESHOLD_MODE_STATIC:
        *sx_pool_mode = SX_COS_BUFFER_MAX_MODE_STATIC_E;
        break;

    case SAI_BUFFER_THRESHOLD_MODE_DYNAMIC:
        *sx_pool_mode = SX_COS_BUFFER_MAX_MODE_DYNAMIC_E;
        break;

    default:
        SX_LOG_ERR("Invalid sai pool mode specified:%d\n", sai_pool_mode);
        return SAI_STATUS_INVALID_PARAMETER;
    }
    return SAI_STATUS_SUCCESS;
}

_Success_(return == SAI_STATUS_SUCCESS)
static sai_status_t convert_sx_pool_mode_to_sai_pool_mode(_In_ sx_cos_buffer_max_mode_e      sx_pool_mode,
                                                          _Out_ sai_buffer_threshold_mode_t* sai_pool_mode)
{
    if (NULL == sai_pool_mode) {
        SX_LOG_ERR("NULL sai_pool_mode\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    switch (sx_pool_mode) {
    case SX_COS_BUFFER_MAX_MODE_STATIC_E:
        *sai_pool_mode = SAI_BUFFER_THRESHOLD_MODE_STATIC;
        break;

    case SX_COS_BUFFER_MAX_MODE_DYNAMIC_E:
        *sai_pool_mode = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC;
        break;

    default:
        SX_LOG_ERR("Invalid sx pool mode specified:%d\n", sx_pool_mode);
        return SAI_STATUS_INVALID_PARAMETER;
    }
    SX_LOG_DBG("sai_pool_mode:%d\n", *sai_pool_mode);
    return SAI_STATUS_SUCCESS;
}

_Success_(return == SAI_STATUS_SUCCESS)
static sai_status_t convert_sx_pool_dir_to_sai_pool_type(_In_ sx_cos_port_buff_pool_direction_e sx_pool_direction,
                                                         _Out_ sai_buffer_pool_type_t         * sai_pool_type)
{
    if (NULL == sai_pool_type) {
        SX_LOG_ERR("NULL sai_pool_type\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SX_COS_PORT_BUFF_POOL_DIRECTION_INGRESS_E == sx_pool_direction) {
        *sai_pool_type = SAI_BUFFER_POOL_INGRESS;
    } else if (SX_COS_PORT_BUFF_POOL_DIRECTION_EGRESS_E == sx_pool_direction) {
        *sai_pool_type = SAI_BUFFER_POOL_EGRESS;
    } else {
        SX_LOG_ERR("Invalid sx_pool direction specified:%d\n", sx_pool_direction);
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_DBG("sai_pool_type:%d\n", *sai_pool_type);
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_create_sai_buffer_profile_id(_In_ uint32_t          db_buffer_profile_ind,
                                                      _Out_ sai_object_id_t* sai_buffer_profile)
{
    sai_status_t sai_status;

    SX_LOG_ENTER();
    if (NULL == sai_buffer_profile) {
        SX_LOG_ERR("NULL sai_buffer_profile\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    sai_status = mlnx_create_object(SAI_OBJECT_TYPE_BUFFER_PROFILE, db_buffer_profile_ind, NULL, sai_buffer_profile);
    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t mlnx_create_sai_pool_id(_In_ uint32_t sx_pool_id, _Out_ sai_object_id_t*    sai_pool)
{
    sai_status_t sai_status;
    char         key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();
    if (NULL == sai_pool) {
        SX_LOG_ERR("NULL sai_pool\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (buffer_limits.num_total_pools <= sx_pool_id) {
        SX_LOG_ERR("Invalid sx_pool_id:%d\n", sx_pool_id);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    sai_status = mlnx_create_object(SAI_OBJECT_TYPE_BUFFER_POOL, sx_pool_id, NULL, sai_pool);
    pool_key_to_str(*sai_pool, key_str);
    SX_LOG_DBG("pool:%s\n", key_str);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_get_sai_buffer_profile_data(_In_ sai_object_id_t               sai_buffer,
                                                     _Out_ uint32_t                   * out_db_buffer_profile_index,
                                                     _Out_ mlnx_sai_buffer_pool_attr_t* sai_pool_attr)
{
    sai_status_t                sai_status;
    uint32_t                    input_db_buffer_profile_index;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr_local;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(sai_buffer, &input_db_buffer_profile_index))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status =
                                   mlnx_get_sai_pool_data(g_sai_buffer_db_ptr->buffer_profiles[
                                                              input_db_buffer_profile_index].sai_pool,
                                                          &sai_pool_attr_local))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (sai_pool_attr) {
        *sai_pool_attr = sai_pool_attr_local;
    }
    if (out_db_buffer_profile_index) {
        *out_db_buffer_profile_index = input_db_buffer_profile_index;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_get_sai_pool_data(_In_ sai_object_id_t               sai_pool,
                                           _Out_ mlnx_sai_buffer_pool_attr_t *sai_pool_attr)
{
    sx_status_t        sx_status;
    sai_status_t       sai_status;
    char               key_str[MAX_KEY_STR_LEN];
    uint32_t           sx_pool_id_local;
    sx_cos_pool_attr_t sx_pool_attr;

    SX_LOG_ENTER();
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    pool_key_to_str(sai_pool, key_str);
    SX_LOG_DBG("sai pool:%s\n", key_str);
    if (NULL == sai_pool_attr) {
        SX_LOG_ERR("NULL sai_pool_attr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(sai_pool, SAI_OBJECT_TYPE_BUFFER_POOL, &sx_pool_id_local, NULL))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (EGRESS_CPU_PORT_SX_POOL_ID == sx_pool_id_local) {
        if (!mlnx_sai_buffer_check_pool_db_consistency(true)) {
            SX_LOG_ERR("e_cpu port wasn't allocated\n");
            SX_LOG_EXIT();
            return SAI_STATUS_ITEM_NOT_FOUND;
        }
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_shared_buff_pool_get(gh_sdk, sx_pool_id_local,  &sx_pool_attr))) {
        SX_LOG_ERR("Failed to get sx pool settings, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_DBG("sx pool_size:%d\n", sx_pool_attr.pool_size);

    sai_pool_attr->sx_pool_id = sx_pool_id_local;
    sai_pool_attr->pool_size  = mlnx_cells_to_bytes(sx_pool_attr.pool_size);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sx_pool_mode_to_sai_pool_mode(sx_pool_attr.mode, &sai_pool_attr->pool_mode))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sx_pool_dir_to_sai_pool_type(sx_pool_attr.pool_dir, &sai_pool_attr->pool_type))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t reset_port_buffer_db_data()
{
    uint32_t                 queue_ind;
    uint32_t                 port_ind;
    sai_status_t             sai_status;
    mlnx_qos_queue_config_t *queue_cfg = NULL;
    mlnx_port_config_t      *port;

    SX_LOG_ENTER();
    memset(g_sai_buffer_db_ptr->buffer_profiles, 0, g_sai_buffer_db_size);

    mlnx_port_foreach(port, port_ind) {
        for (queue_ind = 0; queue_ind < buffer_limits.num_port_queue_buff; queue_ind++) {
            if (SAI_STATUS_SUCCESS != (
                    sai_status =
                        mlnx_queue_cfg_lookup(port->logical, queue_ind, &queue_cfg))) {
                SX_LOG_EXIT();
                return sai_status;
            }
            queue_cfg->buffer_id = SAI_NULL_OBJECT_ID;
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_sai_configure_multicast_buffers_defaults()
{
    sai_status_t                      sai_status;
    sx_status_t                       sx_status;
    uint32_t                          mc_port_logical                = 0;
    sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr   = NULL;
    sx_cos_port_buffer_attr_t       * sx_port_reserved_buff_attr_arr = NULL;
    uint32_t                          count                          = g_resource_limits.shared_buff_mc_max_num_prio;
    uint32_t                          ind;

    SX_LOG_ENTER();
    SX_PORT_TYPE_ID_SET(mc_port_logical, SX_PORT_TYPE_MULTICAST);

    SX_LOG_DBG("Number of MC buffers to configure:%d\n", count);
    sx_port_shared_buff_attr_arr = calloc(count, sizeof(sx_cos_port_shared_buffer_attr_t));
    if (!sx_port_shared_buff_attr_arr) {
        sai_status = SAI_STATUS_NO_MEMORY;
        goto cleanup;
    }
    sx_port_reserved_buff_attr_arr = calloc(count, sizeof(sx_cos_port_buffer_attr_t));
    if (!sx_port_reserved_buff_attr_arr) {
        sai_status = SAI_STATUS_NO_MEMORY;
        goto cleanup;
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_buff_type_get(gh_sdk,
                                                                        mc_port_logical,
                                                                        sx_port_reserved_buff_attr_arr,
                                                                        &count))) {
        SX_LOG_ERR("Failed to get multicast reserved buffers. sx_status:%d, error message:%s\n",
                   sx_status,
                   SX_STATUS_MSG(sx_status));
        sai_status = sdk_to_sai(sx_status);
        goto cleanup;
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_shared_buff_type_get(gh_sdk,
                                                                               mc_port_logical,
                                                                               sx_port_shared_buff_attr_arr,
                                                                               &count))) {
        SX_LOG_ERR("Failed to obtain multicast shared buffers. sx_status:%d, error message%s\n",
                   sx_status, SX_STATUS_MSG(sx_status));
        sai_status = sdk_to_sai(sx_status);
        goto cleanup;
    }
    for (ind = 0; ind < count; ind++) {
        sx_port_reserved_buff_attr_arr[ind].attr.multicast_buff_attr.size =
            SAI_SDK_DEFAULT_MC_BUFFER_RESERVED_SIZE;
        sx_port_shared_buff_attr_arr[ind].attr.multicast_shared_buff_attr.max.max.alpha =
            SX_COS_PORT_BUFF_ALPHA_INFINITY_E;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_sai_buffer_configure_reserved_buffers(mc_port_logical, sx_port_reserved_buff_attr_arr, count))) {
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_sai_buffer_configure_shared_buffers(mc_port_logical, sx_port_shared_buff_attr_arr, count))) {
        goto cleanup;
    }
cleanup:
    if (sx_port_shared_buff_attr_arr) {
        free(sx_port_shared_buff_attr_arr);
    }
    if (sx_port_reserved_buff_attr_arr) {
        free(sx_port_reserved_buff_attr_arr);
    }
    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t mlnx_sai_cleanup_buffer_config()
{
    sai_status_t sai_status;
    uint32_t     mc_port_logical = 0;

    SX_LOG_ENTER();
    SX_PORT_TYPE_ID_SET(mc_port_logical, SX_PORT_TYPE_MULTICAST);

    if (SAI_STATUS_SUCCESS != (sai_status = reset_port_buffer_db_data())) {
        SX_LOG_ERR("Failed resetting buffer db data\n, line:%d", __LINE__);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_unbind_shared_buffers(mc_port_logical))) {
        if (sai_status != SAI_STATUS_ITEM_NOT_FOUND) {
            SX_LOG_ERR("Failed deleting MC shared buffers\n, line:%d", __LINE__);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_unbind_reserved_buffers(mc_port_logical))) {
        if (sai_status != SAI_STATUS_ITEM_NOT_FOUND) {
            SX_LOG_ERR("Failed deleting MC reserved buffers\n, line:%d", __LINE__);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_delete_all_buffer_config())) {
        SX_LOG_ERR("Failed deleting all buffer configuration\n, line:%d", __LINE__);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_reset_all_sx_pools())) {
        SX_LOG_ERR("Failed resetting all pools\n, line:%d", __LINE__);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_delete_all_pools())) {
        SX_LOG_ERR("Failed deleting all pools\n, line:%d", __LINE__);
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_try_create_e_cpu_pool(_Out_ sai_object_id_t *pool_id)
{
    sai_status_t    sai_status;
    sai_object_id_t sai_pool;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_pool_db_alloc_e_cpu_port_slot())) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_pool_id(EGRESS_CPU_PORT_SX_POOL_ID, &sai_pool))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    *pool_id = sai_pool;
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/*
 *  Creates buffer pool by requesting new pool from SDK.
 *  [NOTE][pool#7]:
 *  Last egress pool, which is sdk pool#7 is never deleted physically, because it's used for
 *  sending egress CPU traffic, and must always be available.
 *  For this reason we're simulating creation/deletion of pool#7, by keeping a flag in the pool DB - when sai pool corresponding to pool#7
 *  is created we're marking its DB entry as allocated.
 */
static sai_status_t mlnx_sai_create_buffer_pool(_Out_ sai_object_id_t     * pool_id,
                                                _In_ uint32_t               attr_count,
                                                _In_ const sai_attribute_t *attr_list)
{
    sx_status_t                   sx_status;
    uint32_t                      sx_pool_id;
    sx_cos_pool_attr_t            sx_pool_attr;
    bool                          pool_creation_triggered;
    sai_status_t                  sai_status;
    const sai_attribute_value_t * attr;
    uint32_t                      attr_ind;
    char                          list_str[MAX_LIST_VALUE_STR_LEN] = { 0 };
    sai_buffer_pool_type_t        pool_type;
    sai_buffer_threshold_mode_t   pool_mode;
    sai_object_id_t               sai_pool;
    uint32_t                      pool_size = 0;
    mlnx_sai_buffer_pool_attr_t   sai_pool_attr;

    SX_LOG_ENTER();

    if (NULL == pool_id) {
        SX_LOG_ERR("NULL pool passed in\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    memset(&sai_pool_attr, 0, sizeof(mlnx_sai_buffer_pool_attr_t));
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    if (SAI_STATUS_SUCCESS != (sai_status =
                                   check_attribs_metadata(attr_count, attr_list, pool_attribs, pool_vendor_attribs,
                                                          SAI_COMMON_API_CREATE))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = sai_attr_list_to_str(attr_count, attr_list, pool_attribs, MAX_LIST_VALUE_STR_LEN, list_str))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_NTC("Pool attribs, %s\n", list_str);
    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_POOL_ATTR_TYPE, &attr, &attr_ind);
    assert(SAI_STATUS_SUCCESS == sai_status);
    pool_type  = attr->s32;
    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_POOL_ATTR_SIZE, &attr, &attr_ind);
    assert(SAI_STATUS_SUCCESS == sai_status);
    pool_size = attr->u32;
    if (SAI_STATUS_SUCCESS ==
        (sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_POOL_ATTR_TH_MODE, &attr, &attr_ind))) {
        pool_mode = attr->s32;
    } else {
        pool_mode = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC; /* default */
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_mode_to_sx_pool_mode(pool_mode, &sx_pool_attr.mode))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_type_to_sx_pool_direction(pool_type, &sx_pool_attr.pool_dir))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    sx_pool_attr.pool_size = bytes_to_mlnx_cells(pool_size);
    SX_LOG_DBG("Input bytes:%d, cells:%d\n", pool_size, sx_pool_attr.pool_size);

    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    pool_creation_triggered = mlnx_sai_buffer_get_pool_create_triggered_flag();
    if (!pool_creation_triggered) {
        SX_LOG_NTC(
            "First call to create pool. Will delete all existing pools and buffers before creating new pool now\n");
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_cleanup_buffer_config())) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_save_e_cpu_pool_initial_size())) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_configure_multicast_buffers_defaults())) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_set_pool_raise_triggered_flag())) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    if (SX_STATUS_SUCCESS !=
        (sx_status = sx_api_cos_shared_buff_pool_set(gh_sdk,
                                                     SX_ACCESS_CMD_CREATE,
                                                     &sx_pool_attr,
                                                     &sx_pool_id))) {
        if (SX_STATUS_NO_RESOURCES != sx_status) {
            SX_LOG_ERR("Failed to create sx pool, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
            cl_plock_release(&g_sai_db_ptr->p_lock);
            return sdk_to_sai(sx_status);
        }
        /* if requested pool is egress and pool_7 wasn't allocated yet, then 'allocate' it and return.*/
        if (SAI_BUFFER_POOL_EGRESS == pool_type) {
            if (SAI_STATUS_SUCCESS == (sai_status = mlnx_sai_try_create_e_cpu_pool(&sai_pool))) {
                *pool_id = sai_pool;
            }
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        } else {
            SX_LOG_ERR("Pool creation failed. sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
            cl_plock_release(&g_sai_db_ptr->p_lock);
            return sdk_to_sai(sx_status);
        }
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_pool_id(sx_pool_id, &sai_pool))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    log_sai_pool_data(sai_pool);
    *pool_id = sai_pool;
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_convert_sai_pool_attr_to_sx(_In_ mlnx_sai_buffer_pool_attr_t sai_pool_attr,
                                                                _Out_ sx_cos_pool_attr_t       * sx_pool_attr)
{
    sai_status_t sai_status;

    if (NULL == sx_pool_attr) {
        SX_LOG_ERR("NULL sx_pool_attr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_mode_to_sx_pool_mode(sai_pool_attr.pool_mode, &sx_pool_attr->mode))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_type_to_sx_pool_direction(sai_pool_attr.pool_type, &sx_pool_attr->pool_dir))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    sx_pool_attr->pool_size = bytes_to_mlnx_cells(sai_pool_attr.pool_size);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_remove_buffer_pool(_In_ sai_object_id_t pool_id)
{
    sx_status_t                 sx_status;
    sx_cos_pool_attr_t          sx_pool_attr;
    sai_status_t                sai_status;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(pool_id, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (EGRESS_CPU_PORT_SX_POOL_ID == sai_pool_attr.sx_pool_id) {
        if (!mlnx_sai_buffer_check_pool_db_consistency(true)) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return SAI_STATUS_ITEM_NOT_FOUND;
        }
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_set_e_cpu_pool_state(false))) {
            SX_LOG_ERR("Failed to destroy e_cpu pool, status:%d.\n", sai_status);
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        /* Restore size of  the pool#7 for CPU traffic to its SDK startup value.*/
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_buffer_convert_sai_pool_attr_to_sx(sai_pool_attr, &sx_pool_attr))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_buffer_get_e_cpu_pool_initial_size(&sx_pool_attr.pool_size))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_cos_shared_buff_pool_set(gh_sdk,
                                                         SX_ACCESS_CMD_EDIT,
                                                         &sx_pool_attr,
                                                         &sai_pool_attr.sx_pool_id))) {
            SX_LOG_ERR("Failed to reset e_cpu pool, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
        SX_LOG_DBG("Restored e_cpu pool size to SDK default:%d.\n", sai_pool_attr.sx_pool_id);
    } else {
        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_cos_shared_buff_pool_set(gh_sdk,
                                                         SX_ACCESS_CMD_DESTROY,
                                                         &sx_pool_attr,
                                                         &sai_pool_attr.sx_pool_id))) {
            SX_LOG_ERR("Failed to destroy sx pool, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** buffer pool size in bytes [sai_uint32_t] */
sai_status_t mlnx_sai_set_pool_size_attr(_In_ const sai_object_key_t      * key,
                                         _In_ const sai_attribute_value_t * value,
                                         void                             * arg)
{
    sx_status_t        sx_status;
    uint32_t           sx_pool_id;
    sai_status_t       sai_status;
    sx_cos_pool_attr_t sx_pool_attr;

    SX_LOG_ENTER();
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(key->object_id, SAI_OBJECT_TYPE_BUFFER_POOL, &sx_pool_id, NULL))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SX_STATUS_SUCCESS !=
        (sx_status = sx_api_cos_shared_buff_pool_get(gh_sdk, sx_pool_id,  &sx_pool_attr))) {
        SX_LOG_ERR("Failed to get sx pool settings, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    if (EGRESS_CPU_PORT_SX_POOL_ID == sx_pool_id) {
        cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
        if (!mlnx_sai_buffer_check_pool_db_consistency(true)) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return SAI_STATUS_ITEM_NOT_FOUND;
        }
        cl_plock_release(&g_sai_db_ptr->p_lock);
    }

    SX_LOG_DBG("Input bytes:%d\n", value->u32);
    sx_pool_attr.pool_size = bytes_to_mlnx_cells(value->u32);
    SX_LOG_DBG("Size to set:%d\n", sx_pool_attr.pool_size);
    if (SX_STATUS_SUCCESS !=
        (sx_status = sx_api_cos_shared_buff_pool_set(gh_sdk, SX_ACCESS_CMD_EDIT, &sx_pool_attr, &sx_pool_id))) {
        SX_LOG_ERR("Failed to change sx pool size, sx_status:%d, message %s.\n", sx_status, SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** buffer pool size in bytes [sai_uint32_t] */
static sai_status_t mlnx_sai_get_pool_size_attr(_In_ const sai_object_key_t   * key,
                                                _Inout_ sai_attribute_value_t * value,
                                                _In_ uint32_t                   attr_index,
                                                _Inout_ vendor_cache_t        * cache,
                                                void                          * arg)
{
    sai_status_t                sai_status;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(key->object_id, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_release(&g_sai_db_ptr->p_lock);
    value->u32 = sai_pool_attr.pool_size;
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_set_buffer_pool_attr(_In_ sai_object_id_t pool_id, _In_ const sai_attribute_t * attr)
{
    const sai_object_key_t key = { .object_id = pool_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status;

    SX_LOG_ENTER();

    pool_key_to_str(pool_id, key_str);
    SX_LOG_DBG("SAI pool:%s\n", key_str);
    sai_status = sai_set_attribute(&key, key_str, pool_attribs, pool_vendor_attribs, attr);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_buffer_compute_shared_size(_In_ sai_object_id_t            sai_pool,
                                                        _In_ sx_cos_port_buffer_attr_t* sx_port_reserved_buff_attr_arr,
                                                        _In_ uint32_t                   arr_length,
                                                        _Out_ uint32_t                * total_shared_bytes)
{
    sai_status_t                sai_status;
    uint32_t                    port_ind;
    uint32_t                    buff_ind;
    sx_status_t                 sx_status;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;
    sx_port_log_id_t            mc_port_logical     = 0;
    uint32_t                    total_reserved_size = 0;
    uint32_t                    get_count;
    mlnx_port_config_t         *port;

    SX_LOG_ENTER();
    if (NULL == sx_port_reserved_buff_attr_arr) {
        SX_LOG_ERR("NULL sx_port_reserved_buff_attr_arr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (0 == arr_length) {
        SX_LOG_ERR("0 count\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == total_shared_bytes) {
        SX_LOG_ERR("NULL total_shared_bytes\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    SX_PORT_TYPE_ID_SET(mc_port_logical, SX_PORT_TYPE_MULTICAST);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(sai_pool, &sai_pool_attr))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    mlnx_port_foreach(port, port_ind) {
        memset(sx_port_reserved_buff_attr_arr, 0, arr_length * sizeof(sx_port_reserved_buff_attr_arr[0]));
        get_count = arr_length;
        if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_buff_type_get(gh_sdk,
                                                                            port->logical,
                                                                            sx_port_reserved_buff_attr_arr,
                                                                            &get_count))) {
            SX_LOG_ERR(
                "Failed to get bindings for reserved buffers. port[%d].logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
                port_ind,
                port->logical,
                get_count,
                sx_status,
                SX_STATUS_MSG(sx_status),
                __LINE__);

            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
        assert(get_count <= arr_length);
        for (buff_ind = 0; buff_ind < get_count; buff_ind++) {
            switch (sx_port_reserved_buff_attr_arr[buff_ind].type) {
            case SX_COS_INGRESS_PORT_ATTR_E:
                if (sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_buff_attr.pool_id ==
                    sai_pool_attr.sx_pool_id) {
                    total_reserved_size += sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_buff_attr.size;
                    SX_LOG_DBG("port[%d].logical:%d, ingress port_buff[%d].size:%d\n",
                               port_ind,
                               port->logical,
                               buff_ind,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_buff_attr.size);
                }
                break;

            case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
                if (sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_pg_buff_attr.pool_id ==
                    sai_pool_attr.sx_pool_id) {
                    total_reserved_size +=
                        sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_pg_buff_attr.size;
                    SX_LOG_DBG("port[%d].logical:%d, pg:%d port_buff[%d].size:%d\n",
                               port_ind, port->logical,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_pg_buff_attr.pg,
                               buff_ind,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.ingress_port_pg_buff_attr.size);
                }
                break;

            case SX_COS_EGRESS_PORT_ATTR_E:
                if (sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_buff_attr.pool_id ==
                    sai_pool_attr.sx_pool_id) {
                    total_reserved_size += sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_buff_attr.size;
                    SX_LOG_DBG("port[%d].logical:%d, egress port_buff[%d].size:%d\n",
                               port_ind,
                               port->logical,
                               buff_ind,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_buff_attr.size);
                }
                break;

            case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
                if (sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_tc_buff_attr.pool_id ==
                    sai_pool_attr.sx_pool_id) {
                    total_reserved_size += sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_tc_buff_attr.size;
                    SX_LOG_DBG("port[%d].logical:%d, tc:%d port_buff[%d].size:%d\n",
                               port_ind, port->logical,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_tc_buff_attr.tc,
                               buff_ind,
                               sx_port_reserved_buff_attr_arr[buff_ind].attr.egress_port_tc_buff_attr.size);
                }
                break;

            default:
                SX_LOG_ERR("Invalid buffer type specified:%d\n", sx_port_reserved_buff_attr_arr[buff_ind].type);
                SX_LOG_EXIT();
                return SAI_STATUS_FAILURE;
            }
        }
    }

    memset(sx_port_reserved_buff_attr_arr, 0, arr_length * sizeof(sx_port_reserved_buff_attr_arr[0]));
    get_count = arr_length;
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_buff_type_get(gh_sdk,
                                                                        mc_port_logical,
                                                                        sx_port_reserved_buff_attr_arr,
                                                                        &get_count))) {
        SX_LOG_ERR(
            "Failed to get bindings for MC reserved buffers. MC logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            mc_port_logical,
            get_count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    assert(get_count <= arr_length);
    for (buff_ind = 0; buff_ind < get_count; buff_ind++) {
        if (sx_port_reserved_buff_attr_arr[buff_ind].attr.multicast_buff_attr.pool_id == sai_pool_attr.sx_pool_id) {
            total_reserved_size += sx_port_reserved_buff_attr_arr[buff_ind].attr.multicast_buff_attr.size;
        }
    }
    if (sai_pool_attr.pool_size < mlnx_cells_to_bytes(total_reserved_size)) {
        SX_LOG_ERR("Pool size:%d is less than total reserved sizes:%d. line:%d\n",
                   sai_pool_attr.pool_size,
                   total_reserved_size,
                   __LINE__);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_DBG("pool_id:%d, pool size:%d, total reserved used from the pool:%d, total shared size:%d\n",
               sai_pool_attr.sx_pool_id,
               sai_pool_attr.pool_size,
               total_reserved_size,
               sai_pool_attr.pool_size - total_reserved_size);

    *total_shared_bytes = sai_pool_attr.pool_size - mlnx_cells_to_bytes(total_reserved_size);

    SX_LOG_EXIT();
    return sai_status;
}


/** shared buffer size in bytes [sai_uint32_t].
 * This is derived from substracting all reversed buffers of queue/port
 * from the total pool size. */
static sai_status_t mlnx_sai_get_pool_shared_size_attr(_In_ const sai_object_key_t   * key,
                                                       _Inout_ sai_attribute_value_t * value,
                                                       _In_ uint32_t                   attr_index,
                                                       _Inout_ vendor_cache_t        * cache,
                                                       void                          * arg)
{
    sx_cos_port_buffer_attr_t* sx_port_reserved_buff_attr_arr;
    uint32_t                   count = buffer_limits.max_buffers_per_port;
    sai_status_t               sai_status;

    sx_port_reserved_buff_attr_arr = calloc(count, sizeof(sx_cos_port_buffer_attr_t));
    if (!sx_port_reserved_buff_attr_arr) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    sai_status =
        mlnx_sai_buffer_compute_shared_size(key->object_id, sx_port_reserved_buff_attr_arr, count, &(value->u32));
    cl_plock_release(&g_sai_db_ptr->p_lock);
    free(sx_port_reserved_buff_attr_arr);
    SX_LOG_EXIT();
    return sai_status;
}

/** buffer pool type [sai_buffer_pool_type_t] */
static sai_status_t mlnx_sai_get_pool_type_attr(_In_ const sai_object_key_t   * key,
                                                _Inout_ sai_attribute_value_t * value,
                                                _In_ uint32_t                   attr_index,
                                                _Inout_ vendor_cache_t        * cache,
                                                void                          * arg)
{
    sai_status_t                sai_status;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(key->object_id, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->s32 = sai_pool_attr.pool_type;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** shared threshold mode for the buffer pool [sai_buffer_threadhold_mode_t]
 * (default to SAI_BUFFER_POOL_DYNAMIC_TH) */
static sai_status_t mlnx_sai_get_pool_th_mode_attr(_In_ const sai_object_key_t   * key,
                                                   _Inout_ sai_attribute_value_t * value,
                                                   _In_ uint32_t                   attr_index,
                                                   _Inout_ vendor_cache_t        * cache,
                                                   void                          * arg)
{
    sai_status_t                sai_status;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(key->object_id, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->s32 = sai_pool_attr.pool_mode;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_get_buffer_pool_attr(_In_ sai_object_id_t      pool_id,
                                                  _In_ uint32_t             attr_count,
                                                  _Inout_ sai_attribute_t * attr_list)
{
    const sai_object_key_t key = { .object_id = pool_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status;

    SX_LOG_ENTER();
    pool_key_to_str(pool_id, key_str);
    SX_LOG_DBG("SAI pool:%s\n", key_str);

    sai_status = sai_get_attributes(&key, key_str, pool_attribs, pool_vendor_attribs, attr_count, attr_list);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t db_buffer_profile_reserve_entry(_Out_ uint32_t* buff_profile_db_ind_out)
{
    sai_status_t sai_status          = SAI_STATUS_SUCCESS;
    uint32_t     buff_profile_db_ind = 0;
    uint32_t     buff_profile_db_cnt = mlnx_sai_get_buffer_profile_number();

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    for (buff_profile_db_ind = SENTINEL_BUFFER_DB_ENTRY_INDEX + 1;
         buff_profile_db_ind < buff_profile_db_cnt;
         buff_profile_db_ind++) {
        if (!g_sai_buffer_db_ptr->buffer_profiles[buff_profile_db_ind].is_valid) {
            break;
        }
    }
    if (buff_profile_db_ind == buff_profile_db_cnt) {
        SX_LOG_ERR("Buffer profile db full\n");
        cl_plock_release(&g_sai_db_ptr->p_lock);
        return SAI_STATUS_TABLE_FULL;
    }
    g_sai_buffer_db_ptr->buffer_profiles[buff_profile_db_ind].is_valid = true;
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);

    *buff_profile_db_ind_out = buff_profile_db_ind;
    SX_LOG_DBG("Reserved buffer profile db item_ind:0x%X\n", buff_profile_db_ind);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_create_buffer_profile(_Out_ sai_object_id_t     * buffer_profile_id,
                                                   _In_ uint32_t               attr_count,
                                                   _In_ const sai_attribute_t *attr_list)
{
    uint32_t                           buff_profile_db_ind = 0;
    mlnx_sai_db_buffer_profile_entry_t new_buffer_profile;
    char                               list_str[MAX_LIST_VALUE_STR_LEN] = { 0 };
    sai_status_t                       sai_status;
    const sai_attribute_value_t      * attr;
    uint32_t                           attr_ind;
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr;
    bool                               mode_set = false;

    SX_LOG_ENTER();
    memset(&new_buffer_profile, 0, sizeof(new_buffer_profile));
    if (NULL == buffer_profile_id) {
        SX_LOG_ERR("NULL key passed in\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = check_attribs_metadata(
             attr_count,
             attr_list,
             buffer_profile_attribs,
             buffer_profile_vendor_attribs,
             SAI_COMMON_API_CREATE))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = sai_attr_list_to_str(
             attr_count,
             attr_list,
             buffer_profile_attribs,
             MAX_LIST_VALUE_STR_LEN,
             list_str))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_NTC("Buffer profile attribs, %s\n", list_str);
    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_PROFILE_ATTR_POOL_ID, &attr, &attr_ind);
    assert(SAI_STATUS_SUCCESS == sai_status);

    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(attr->oid, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_release(&g_sai_db_ptr->p_lock);

    new_buffer_profile.sai_pool = attr->oid;
    sai_status                  = find_attrib_in_list(attr_count,
                                                      attr_list,
                                                      SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE,
                                                      &attr,
                                                      &attr_ind);
    assert(SAI_STATUS_SUCCESS == sai_status);
    new_buffer_profile.reserved_size = attr->u32;

    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH, &attr,
                                 &attr_ind))) {
        if (SAI_BUFFER_THRESHOLD_MODE_DYNAMIC != sai_pool_attr.pool_mode) {
            SX_LOG_ERR("Dynamic threshold size cannot be passed to buffer profile when Input pool is not dynamic.\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTRIBUTE_0 + attr_ind;
        }
        new_buffer_profile.shared_max.mode      = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC;
        new_buffer_profile.shared_max.max.alpha = attr->u8;
        mode_set                                = true;
    }
    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH, &attr, &attr_ind))) {
        if (mode_set) {
            SX_LOG_ERR("Both static and dynamic thresholds cannot be specified.\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTRIBUTE_0 + attr_ind;
        }

        if (SAI_BUFFER_THRESHOLD_MODE_STATIC != sai_pool_attr.pool_mode) {
            SX_LOG_ERR("Static threshold size cannot be passed to buffer profile when Input pool is not static.\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTRIBUTE_0 + attr_ind;
        }
        new_buffer_profile.shared_max.mode          = SAI_BUFFER_THRESHOLD_MODE_STATIC;
        new_buffer_profile.shared_max.max.static_th = attr->u32;
        mode_set                                    = true;
    }
    if (!mode_set) {
        SX_LOG_ERR("One of threshold modes must be specified.\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_ATTRIBUTE_0 + attr_ind;
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_PROFILE_ATTR_XOFF_TH, &attr, &attr_ind))) {
        new_buffer_profile.xoff = attr->u32;
    } else {
        new_buffer_profile.xoff = 0; /*default*/
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status = find_attrib_in_list(attr_count, attr_list, SAI_BUFFER_PROFILE_ATTR_XON_TH, &attr, &attr_ind))) {
        new_buffer_profile.xon = attr->u32;
    } else {
        new_buffer_profile.xon = 0; /*default*/
    }

    if (SAI_STATUS_SUCCESS != (sai_status = db_buffer_profile_reserve_entry(&buff_profile_db_ind))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    new_buffer_profile.is_valid                               = true;
    g_sai_buffer_db_ptr->buffer_profiles[buff_profile_db_ind] = new_buffer_profile;
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_create_sai_buffer_profile_id(buff_profile_db_ind, buffer_profile_id))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_DBG("New buffer profile id:0x%" PRIx64 "\n", *buffer_profile_id);
    log_sai_buffer_profile_db_entry(buff_profile_db_ind);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_remove_buffer_profile(_In_ sai_object_id_t buffer_profile_id)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    if (SAI_NULL_OBJECT_ID == buffer_profile_id) {
        SX_LOG_DBG("NULL Buffer profile\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_buffer_profile_data(buffer_profile_id, &db_buffer_profile_index, NULL))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_is_buffer_in_use(buffer_profile_id))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }

    memset(&g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index],
           0,
           sizeof(mlnx_sai_db_buffer_profile_entry_t));
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** pointer to buffer pool object id [sai_object_id_t] */
static sai_status_t mlnx_sai_get_buffer_profile_pool_id_attr(_In_ const sai_object_key_t   * key,
                                                             _Inout_ sai_attribute_value_t * value,
                                                             _In_ uint32_t                   attr_index,
                                                             _Inout_ vendor_cache_t        * cache,
                                                             void                          * arg)
{
    sai_status_t                sai_status;
    uint32_t                    db_buffer_profile_index;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;

    SX_LOG_ENTER();
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_buffer_profile_data(key->object_id, &db_buffer_profile_index, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->oid = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].sai_pool;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** pointer to buffer pool object id [sai_object_id_t]
 *  NOTE: Pool id = SAI_NULL_OBJECT_ID is not a valid case for MELLANOX's implementation. */
static sai_status_t mlnx_sai_set_buffer_profile_pool_id_attr(_In_ const sai_object_key_t      * key,
                                                             _In_ const sai_attribute_value_t * value,
                                                             void                             * arg)
{
    char                        key_str[MAX_KEY_STR_LEN];
    sai_status_t                sai_status;
    mlnx_sai_buffer_pool_attr_t cur_sai_pool_attr;
    mlnx_sai_buffer_pool_attr_t new_sai_pool_attr;
    uint32_t                    db_buffer_profile_index;
    sai_object_id_t             prev_pool = SAI_NULL_OBJECT_ID;

    SX_LOG_ENTER();
    buffer_profile_key_to_str(key->object_id, key_str);
    SX_LOG_DBG("Buffer profile:%s\n", key_str);
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_get_sai_buffer_profile_data(key->object_id, &db_buffer_profile_index, &cur_sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(value->oid, &new_sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_status = mlnx_sai_is_buffer_in_use(key->object_id);
    if (SAI_STATUS_OBJECT_IN_USE == sai_status) {
        /* Changing pool attribute:
         *   1. buffer profile cannot change direction, otherwise underlying PGs or queues all can get suddently attached to a wrong pool.
         *   2. changing pool on buffer profile, means all entities attached to buffer profile will need to change as well.
         *   The whole dependency tree of attachments to buffer profile need to be processed.
         */
        if (new_sai_pool_attr.pool_type != cur_sai_pool_attr.pool_type) {
            SX_LOG_ERR("Invalid pool specified for a set operation\n");
            pool_key_to_str(value->oid, key_str);
            SX_LOG_DBG("%s\n", key_str);
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    } else if (SAI_STATUS_SUCCESS != sai_status) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].sai_pool != value->oid) {
        prev_pool = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].sai_pool;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].sai_pool = value->oid;
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, prev_pool))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** reserved buffer size in bytes [sai_uint32_t] */
static sai_status_t mlnx_sai_get_buffer_profile_size_attr(_In_ const sai_object_key_t   * key,
                                                          _Inout_ sai_attribute_value_t * value,
                                                          _In_ uint32_t                   attr_index,
                                                          _Inout_ vendor_cache_t        * cache,
                                                          void                          * arg)
{
    char         key_str[MAX_KEY_STR_LEN];
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_ind;

    SX_LOG_ENTER();
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_buffer_profile_data(key->object_id, &db_buffer_profile_ind, NULL))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    buffer_profile_key_to_str(key->object_id, key_str);
    SX_LOG_DBG("Buffer profile:%s\n", key_str);
    value->u32 = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_ind].reserved_size;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/*
 *  Description
 *   Applies settings of buffer profile entry to SDK buffers which are identified by affected_items structure.
 *
 *   affected_items contains bool flags for each pg/tc/i_port_buffer/e_port_buffer.
 *   If an entry is true, it means given item should be applied with new buffer profile settings.
 */
static void mlnx_sai_buffer_apply_profile_to_reserved_structs(
    _Inout_ sx_cos_port_buffer_attr_t       * sx_port_reserved_buff_attr_arr,
    _In_ uint32_t                             count,
    _In_ mlnx_sai_db_buffer_profile_entry_t   buff_db_entry,
    _In_ mlnx_sai_buffer_pool_attr_t          sai_pool_attr,
    _In_ mlnx_affect_port_buff_items_t const* affected_items)
{
    uint32_t ind;

    SX_LOG_ENTER();
    SX_LOG_DBG("count:%d\n", count);
    for (ind = 0; ind < count; ind++) {
        switch (sai_pool_attr.pool_type) {
        case SAI_BUFFER_POOL_INGRESS:
            switch (sx_port_reserved_buff_attr_arr[ind].type) {
            case SX_COS_INGRESS_PORT_ATTR_E:
                assert(
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_buff_attr.pool_id <
                    buffer_limits.num_ingress_pools);
                if (true ==
                    affected_items->i_port_buffers[sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_buff_attr.
                                                   pool_id]) {
                    SX_LOG_DBG("(i)port_buffers[%d], pool_id:%d\n",
                               ind,
                               sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_buff_attr.pool_id);
                    assert(SAI_BUFFER_POOL_INGRESS == sai_pool_attr.pool_type);
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_buff_attr.size = bytes_to_mlnx_cells(
                        buff_db_entry.reserved_size);
                    log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
                }
                break;

            case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:

                if (sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.pg >=
                    mlnx_sai_get_buffer_resource_limits()->num_port_pg_buff) {
                    /* control traffic is not supported */
                    continue;
                }
                if (true ==
                    affected_items->pgs[sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.pg]) {
                    SX_LOG_DBG("item:%d, pg:%d\n",
                               ind,
                               sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.pg);
                    assert(SAI_BUFFER_POOL_INGRESS == sai_pool_attr.pool_type);
                    log_sai_buffer_profile_db_entry_fields(buff_db_entry);
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.size = bytes_to_mlnx_cells(
                        buff_db_entry.reserved_size);
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.is_lossy =
                        ((0 == buff_db_entry.xoff) && (0 == buff_db_entry.xon));
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.xon = bytes_to_mlnx_cells(
                        buff_db_entry.xon);
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.xoff = bytes_to_mlnx_cells(
                        buff_db_entry.reserved_size - buff_db_entry.xoff);
                    sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
                }
                break;

            default:
                break;
            }
            break;

        case SAI_BUFFER_POOL_EGRESS:
            switch (sx_port_reserved_buff_attr_arr[ind].type) {
            case SX_COS_EGRESS_PORT_ATTR_E:
                SX_LOG_DBG("pool_id:%d\n", sx_port_reserved_buff_attr_arr[ind].attr.egress_port_buff_attr.pool_id);
                assert((sx_port_reserved_buff_attr_arr[ind].attr.egress_port_buff_attr.pool_id -
                        DEFAULT_EGRESS_SX_POOL_ID) < mlnx_sai_get_buffer_resource_limits()->num_egress_pools);
                if (true ==
                    affected_items->e_port_buffers[sx_port_reserved_buff_attr_arr[ind].attr.egress_port_buff_attr.
                                                   pool_id -
                                                   DEFAULT_EGRESS_SX_POOL_ID]) {
                    SX_LOG_DBG("(e)port_buffers[%d], pool_id:%d\n",
                               ind,
                               sx_port_reserved_buff_attr_arr[ind].attr.egress_port_buff_attr.pool_id);
                    assert(SAI_BUFFER_POOL_EGRESS == sai_pool_attr.pool_type);
                    sx_port_reserved_buff_attr_arr[ind].attr.egress_port_buff_attr.size = bytes_to_mlnx_cells(
                        buff_db_entry.reserved_size);
                    log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
                }
                break;

            case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
                if (sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.tc >=
                    mlnx_sai_get_buffer_resource_limits()->num_port_queue_buff) {
                    /* control traffic is not supported */
                    continue;
                }
                if (true ==
                    affected_items->tcs[sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.tc]) {
                    SX_LOG_DBG("item:%d, queue:%d\n",
                               ind,
                               sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.tc);
                    assert(SAI_BUFFER_POOL_EGRESS == sai_pool_attr.pool_type);
                    sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.size = bytes_to_mlnx_cells(
                        buff_db_entry.reserved_size);
                    sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
                }
                break;

            default:
                break;
            }
            break;
        }
    }
    SX_LOG_EXIT();
}

static sai_status_t mlnx_sai_shared_max_to_sx(_Inout_ sx_cos_buffer_max_t   * sx_cos_buff_max,
                                              _In_ mlnx_sai_shared_max_size_t sai_shared_max)
{
    if (SAI_BUFFER_THRESHOLD_MODE_DYNAMIC == sai_shared_max.mode) {
        return mlnx_buffer_convert_alpha_sai_to_sx(sai_shared_max.max.alpha, &sx_cos_buff_max->max.alpha);
    } else {
        sx_cos_buff_max->max.size = bytes_to_mlnx_cells(sai_shared_max.max.static_th);
        return SAI_STATUS_SUCCESS;
    }
}

static sai_status_t mlnx_sai_buffer_apply_profile_to_shared_structs(
    _Inout_ sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr,
    _In_ uint32_t                             count,
    _In_ mlnx_sai_db_buffer_profile_entry_t   buff_db_entry,
    _In_ mlnx_sai_buffer_pool_attr_t          sai_pool_attr,
    _In_ mlnx_affect_port_buff_items_t const* affected_items)
{
    sai_status_t             sai_status;
    sx_cos_buffer_max_mode_e sx_pool_mode;
    uint32_t                 ind;

    SX_LOG_ENTER();
    SX_LOG_DBG("count:%d\n", count);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_mode_to_sx_pool_mode(sai_pool_attr.pool_mode, &sx_pool_mode))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    for (ind = 0; ind < count; ind++) {
        switch (sai_pool_attr.pool_type) {
        case SAI_BUFFER_POOL_INGRESS:
            switch (sx_port_shared_buff_attr_arr[ind].type) {
            case SX_COS_INGRESS_PORT_ATTR_E:
                if (true ==
                    affected_items->i_port_buffers[sx_port_shared_buff_attr_arr[ind].attr.ingress_port_shared_buff_attr
                                                   .
                                                   pool_id]) {
                    SX_LOG_DBG("(i)port_buffers[%d], pool_id:%d\n",
                               ind,
                               sx_port_shared_buff_attr_arr[ind].attr.ingress_port_shared_buff_attr.pool_id);
                    sx_port_shared_buff_attr_arr[ind].attr.ingress_port_shared_buff_attr.max.mode = sx_pool_mode;
                    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                                   &sx_port_shared_buff_attr_arr[ind].attr.
                                                   ingress_port_shared_buff_attr.max,
                                                   buff_db_entry.shared_max))) {
                        SX_LOG_EXIT();
                        return sai_status;
                    }
                    sx_port_shared_buff_attr_arr[ind].attr.ingress_port_shared_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
                }
                break;

            case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
                if (sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pg >=
                    mlnx_sai_get_buffer_resource_limits()->num_port_pg_buff) {
                    /* control traffic is not supported */
                    continue;
                }
                if (true ==
                    affected_items->pgs[sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pg]) {
                    SX_LOG_DBG("item:%d, pg[%d]:\n",
                               ind,
                               sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pg);
                    sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.max.mode = sx_pool_mode;
                    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                                   &sx_port_shared_buff_attr_arr[ind].attr.
                                                   ingress_port_pg_shared_buff_attr.max,
                                                   buff_db_entry.shared_max))) {
                        SX_LOG_EXIT();
                        return sai_status;
                    }
                    sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
                }
                break;

            default:
                break;
            }
            break;

        case SAI_BUFFER_POOL_EGRESS:
            switch (sx_port_shared_buff_attr_arr[ind].type) {
            case SX_COS_EGRESS_PORT_ATTR_E:
                assert((sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr.pool_id -
                        DEFAULT_EGRESS_SX_POOL_ID) < buffer_limits.num_egress_pools);
                if (true ==
                    affected_items->e_port_buffers[sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr.
                                                   pool_id - DEFAULT_EGRESS_SX_POOL_ID]) {
                    SX_LOG_DBG("(e)port_buffers[%d], pool_id:%d\n",
                               ind,
                               sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr.pool_id);
                    sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr.max.mode = sx_pool_mode;
                    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                                   &sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr
                                                   .max,
                                                   buff_db_entry.shared_max))) {
                        SX_LOG_EXIT();
                        return sai_status;
                    }
                    sx_port_shared_buff_attr_arr[ind].attr.egress_port_shared_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
                }
                break;

            case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
                if (sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.tc >=
                    mlnx_sai_get_buffer_resource_limits()->num_port_queue_buff) {
                    /* control traffic is not supported */
                    continue;
                }
                if (true ==
                    affected_items->tcs[sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.tc]) {
                    SX_LOG_DBG("item:%d, queue:%d\n",
                               ind,
                               sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.tc);
                    sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.max.mode = sx_pool_mode;
                    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                                   &sx_port_shared_buff_attr_arr[ind].attr.
                                                   egress_port_tc_shared_buff_attr.max,
                                                   buff_db_entry.shared_max))) {
                        SX_LOG_EXIT();
                        return sai_status;
                    }
                    sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.pool_id =
                        sai_pool_attr.sx_pool_id;
                    log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
                }
                break;

            default:
                break;
            }
            break;
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_prepare_affected_reserved_buffers_for_reset(
    _Inout_ sx_cos_port_buffer_attr_t * sx_port_reserved_buff_attr_arr,
    _In_ uint32_t                       count)
{
    uint32_t ind;

    SX_LOG_ENTER();

    SX_LOG_DBG("count:%d\n", count);
    for (ind = 0; ind < count; ind++) {
        switch (sx_port_reserved_buff_attr_arr[ind].type) {
        case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
            if (sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.pg >=
                mlnx_sai_get_buffer_resource_limits()->num_port_pg_buff) {
                /* control traffic is not supported */
                continue;
            }
            sx_port_reserved_buff_attr_arr[ind].attr.ingress_port_pg_buff_attr.size = 0;
            break;

        case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
            if (sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.tc >=
                mlnx_sai_get_buffer_resource_limits()->num_port_queue_buff) {
                /* control traffic is not supported */
                continue;
            }
            sx_port_reserved_buff_attr_arr[ind].attr.egress_port_tc_buff_attr.size = 0;
            break;

        default:
            /*  Only PG and TC buffers are eligible for pool_id change.
             *   Port.pool buffers never change pool_id values */
            SX_LOG_ERR("Invalid buffer type:%d\n", sx_port_reserved_buff_attr_arr[ind].type);
            log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
            return SAI_STATUS_INVALID_PARAMETER;
        }
        log_sx_port_buffers(0, 1, &sx_port_reserved_buff_attr_arr[ind]);
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_prepare_affected_shared_buffers_for_reset(
    _In_ mlnx_sai_buffer_pool_attr_t          sai_pool_attr,
    _Inout_ sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr,
    _In_ uint32_t                             count,
    _In_ mlnx_sai_shared_max_size_t           shared_max)
{
    sai_status_t             sai_status;
    uint32_t                 ind;
    sx_cos_buffer_max_mode_e sx_pool_mode;

    SX_LOG_ENTER();
    SX_LOG_DBG("count:%d\n", count);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = convert_sai_pool_mode_to_sx_pool_mode(sai_pool_attr.pool_mode, &sx_pool_mode))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    for (ind = 0; ind < count; ind++) {
        switch (sx_port_shared_buff_attr_arr[ind].type) {
        case SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E:
            if (sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pg >=
                mlnx_sai_get_buffer_resource_limits()->num_port_pg_buff) {
                /* control traffic is not supported */
                continue;
            }
            SX_LOG_DBG("item:%d, pg[%d]:\n",
                       ind, sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.pg);
            if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                           &sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.max,
                                           shared_max))) {
                SX_LOG_EXIT();
                return sai_status;
            }
            sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.max.max.size = 0;
            sx_port_shared_buff_attr_arr[ind].attr.ingress_port_pg_shared_buff_attr.max.mode     = sx_pool_mode;
            break;

        case SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E:
            if (sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.tc >=
                mlnx_sai_get_buffer_resource_limits()->num_port_queue_buff) {
                /* control traffic is not supported */
                continue;
            }
            if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_shared_max_to_sx(
                                           &sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.max,
                                           shared_max))) {
                SX_LOG_EXIT();
                return sai_status;
            }
            SX_LOG_DBG("item:%d, queue:%d\n",
                       ind,
                       sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.tc);
            sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.max.max.size = 0;
            sx_port_shared_buff_attr_arr[ind].attr.egress_port_tc_shared_buff_attr.max.mode     = sx_pool_mode;
            break;

        default:
            /*  Only PG and TC buffers are eligible for pool_id change.
             *   Port.pool buffers never change pool_id values */
            SX_LOG_ERR("Invalid shared buffer type:%d\n", sx_port_shared_buff_attr_arr[ind].type);
            log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
            return SAI_STATUS_INVALID_PARAMETER;
        }
        log_sx_port_shared_buffers(0, 1, &sx_port_shared_buff_attr_arr[ind]);
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_prepare_affected_buffers(_In_ mlnx_sai_buffer_pool_attr_t          sai_pool_attr,
                                                             _In_ mlnx_affect_port_buff_items_t const *affected_items,
                                                             _Out_ sx_cos_port_shared_buffer_attr_t   *sx_port_shared_buff_attr_arr,
                                                             _Out_ sx_cos_port_buffer_attr_t          *sx_port_reserved_buff_attr_arr)
{
    uint32_t cos_buffer_ind = 0;
    uint32_t ind;

    SX_LOG_ENTER();
    SX_LOG_DBG("pool_type:%d\n", sai_pool_attr.pool_type);
    log_sai_pool_attribs(sai_pool_attr);
    switch (sai_pool_attr.pool_type) {
    case SAI_BUFFER_POOL_INGRESS:
        for (ind = 0; ind < buffer_limits.num_port_pg_buff; ind++) {
            if (affected_items->pgs[ind]) {
                assert(cos_buffer_ind < affected_items->affected_count);
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.ingress_port_pg_buff_attr.pg      = ind;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.ingress_port_pg_buff_attr.pool_id =
                    sai_pool_attr.sx_pool_id;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].type =
                    SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.ingress_port_pg_shared_buff_attr.pg      = ind;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.ingress_port_pg_shared_buff_attr.pool_id =
                    sai_pool_attr.sx_pool_id;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].type =
                    SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E;
                cos_buffer_ind++;
            }
        }
        for (ind = 0; ind < buffer_limits.num_ingress_pools; ind++) {
            if (affected_items->i_port_buffers[ind]) {
                assert(cos_buffer_ind < affected_items->affected_count);
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.ingress_port_buff_attr.pool_id = ind;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].type                                =
                    SX_COS_INGRESS_PORT_ATTR_E;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.ingress_port_shared_buff_attr.pool_id = ind;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].type                                       =
                    SX_COS_INGRESS_PORT_ATTR_E;
                cos_buffer_ind++;
            }
        }
        break;

    case SAI_BUFFER_POOL_EGRESS:
        for (ind = 0; ind < buffer_limits.num_port_queue_buff; ind++) {
            if (affected_items->tcs[ind]) {
                assert(cos_buffer_ind < affected_items->affected_count);
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.egress_port_tc_buff_attr.pool_id =
                    sai_pool_attr.sx_pool_id;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.egress_port_tc_buff_attr.tc = ind;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].type                             =
                    SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.egress_port_tc_shared_buff_attr.tc = ind;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].type                                    =
                    SX_COS_EGRESS_PORT_TRAFFIC_CLASS_ATTR_E;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.egress_port_tc_shared_buff_attr.pool_id =
                    sai_pool_attr.sx_pool_id;
                cos_buffer_ind++;
            }
        }
        for (ind = 0; ind < buffer_limits.num_egress_pools; ind++) {
            if (affected_items->e_port_buffers[ind]) {
                assert(cos_buffer_ind < affected_items->affected_count);
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].attr.egress_port_buff_attr.pool_id = ind +
                                                                                                    DEFAULT_EGRESS_SX_POOL_ID;
                sx_port_reserved_buff_attr_arr[cos_buffer_ind].type =
                    SX_COS_EGRESS_PORT_ATTR_E;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].attr.egress_port_shared_buff_attr.pool_id = ind +
                                                                                                         DEFAULT_EGRESS_SX_POOL_ID;
                sx_port_shared_buff_attr_arr[cos_buffer_ind].type =
                    SX_COS_EGRESS_PORT_ATTR_E;
                cos_buffer_ind++;
            }
        }
        break;

    default:
        SX_LOG_ERR("Invalid pool type:%d\n", sai_pool_attr.pool_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_DBG("cos_buffer_ind.1:%d\n", cos_buffer_ind);
    assert(affected_items->affected_count == cos_buffer_ind);
    SX_LOG_DBG("[start]:constructed buffers\n");
    SX_LOG_DBG("buffer count:%d\n", affected_items->affected_count);
    log_sx_port_buffers(0, affected_items->affected_count, sx_port_reserved_buff_attr_arr);
    log_sx_port_shared_buffers(0, affected_items->affected_count, sx_port_shared_buff_attr_arr);
    SX_LOG_DBG("[end]:constructed buffers\n");
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_apply_buffer_settings_to_port(_In_ sx_port_log_id_t                     log_port,
                                                           _In_ mlnx_sai_db_buffer_profile_entry_t   buff_db_entry,
                                                           _In_ mlnx_affect_port_buff_items_t const* affected_items,
                                                           _In_ sai_object_id_t                      prev_pool)
{
    sai_status_t                      sai_status;
    sx_status_t                       sx_status;
    sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr;
    sx_cos_port_buffer_attr_t       * sx_port_reserved_buff_attr_arr;
    uint32_t                          count;
    uint32_t                          db_port_ind = UINT_MAX;
    mlnx_port_config_t               *port;
    mlnx_sai_buffer_pool_attr_t       prev_pool_attr;
    mlnx_sai_buffer_pool_attr_t       new_pool_attr;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(buff_db_entry.sai_pool, &new_pool_attr))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (prev_pool != SAI_NULL_OBJECT_ID) {
        SX_LOG_DBG("prev_pool:0x%" PRIx64 "\n", prev_pool);
        sai_status = mlnx_get_sai_pool_data(prev_pool, &prev_pool_attr);
        if (SAI_STATUS_SUCCESS != sai_status) {
            SX_LOG_EXIT();
            return sai_status;
        }
        log_sai_pool_attribs(prev_pool_attr);
    }
    mlnx_port_foreach(port, db_port_ind) {
        if (port->logical == log_port) {
            break;
        }
    }
    count = affected_items->affected_count;
    SX_LOG_DBG("count.1:%d\n", count);
    assert(count <= buffer_limits.max_buffers_per_port);
    sx_port_shared_buff_attr_arr = calloc(count, sizeof(sx_cos_port_shared_buffer_attr_t));
    if (!sx_port_shared_buff_attr_arr) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    sx_port_reserved_buff_attr_arr = calloc(count, sizeof(sx_cos_port_buffer_attr_t));
    if (!sx_port_reserved_buff_attr_arr) {
        free(sx_port_shared_buff_attr_arr);
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }

    /* create only those buffers which are affected */
    if (SAI_STATUS_SUCCESS != (
            sai_status = mlnx_sai_buffer_prepare_affected_buffers(
                (SAI_NULL_OBJECT_ID != prev_pool) ? prev_pool_attr : new_pool_attr,
                affected_items,
                sx_port_shared_buff_attr_arr,
                sx_port_reserved_buff_attr_arr))) {
        SX_LOG_ERR("Failed to prepare affected buffers, status:%d\n", sai_status);
        free(sx_port_shared_buff_attr_arr);
        free(sx_port_reserved_buff_attr_arr);
        return sai_status;
    }

    if (prev_pool != SAI_NULL_OBJECT_ID) {
        /* reset buffers to be able to apply pool_id change */
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_prepare_affected_reserved_buffers_for_reset(sx_port_reserved_buff_attr_arr, count))) {
            free(sx_port_shared_buff_attr_arr);
            free(sx_port_reserved_buff_attr_arr);
            SX_LOG_EXIT();
            return sai_status;
        }

        /*  if there are any pool_id changes then need to set sizes to 0 to be able to change the pool_id
         *   Since this is 1 call, pass all buffers.
         */
        sx_status = sx_api_cos_port_buff_type_set(gh_sdk,
                                                  SX_ACCESS_CMD_SET,
                                                  log_port,
                                                  sx_port_reserved_buff_attr_arr,
                                                  count);
        if (SX_STATUS_SUCCESS != sx_status) {
            SX_LOG_ERR(
                "Failed to reset bindings for reserved buffers. port.logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
                log_port,
                count,
                sx_status,
                SX_STATUS_MSG(sx_status),
                __LINE__);
            free(sx_port_shared_buff_attr_arr);
            free(sx_port_reserved_buff_attr_arr);
            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
        SX_LOG_DBG(
            "Removed bindings for sx reserved buffers for port.logical:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
            log_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
    }

    mlnx_sai_buffer_apply_profile_to_reserved_structs(
        sx_port_reserved_buff_attr_arr,
        count,
        buff_db_entry,
        new_pool_attr,
        affected_items
        );

    count = affected_items->affected_count;
    if (prev_pool != SAI_NULL_OBJECT_ID) {
        /* reset buffers to be able to change the pool_id */
        if (SAI_STATUS_SUCCESS != (
                sai_status =
                    mlnx_sai_prepare_affected_shared_buffers_for_reset(prev_pool_attr, sx_port_shared_buff_attr_arr,
                                                                       count,
                                                                       buff_db_entry.shared_max))) {
            SX_LOG_ERR("Failed to prepare affected buffers, status:%d\n", sai_status);
            free(sx_port_shared_buff_attr_arr);
            free(sx_port_reserved_buff_attr_arr);
            return sai_status;
        }
        sx_status = sx_api_cos_port_shared_buff_type_set(gh_sdk,
                                                         SX_ACCESS_CMD_SET,
                                                         log_port,
                                                         sx_port_shared_buff_attr_arr,
                                                         count);
        if (SX_STATUS_SUCCESS != sx_status) {
            SX_LOG_ERR(
                "Failed to reset bindings for shared buffers. port.logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
                log_port,
                count,
                sx_status,
                SX_STATUS_MSG(sx_status),
                __LINE__);
            free(sx_port_shared_buff_attr_arr);
            free(sx_port_reserved_buff_attr_arr);
            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
        SX_LOG_DBG(
            "Removed bindings for sx shared buffers for port.logical:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
            log_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
    }
    if (SAI_STATUS_SUCCESS != (sai_status =
                                   mlnx_sai_buffer_apply_profile_to_shared_structs(
                                       sx_port_shared_buff_attr_arr,
                                       count,
                                       buff_db_entry,
                                       new_pool_attr,
                                       affected_items))) {
        free(sx_port_shared_buff_attr_arr);
        free(sx_port_reserved_buff_attr_arr);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_configure_reserved_buffers(log_port, sx_port_reserved_buff_attr_arr, count))) {
        log_sx_port_buffers(db_port_ind, count, sx_port_reserved_buff_attr_arr);
        free(sx_port_shared_buff_attr_arr);
        free(sx_port_reserved_buff_attr_arr);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_configure_shared_buffers(log_port, sx_port_shared_buff_attr_arr, count))) {
        log_sx_port_shared_buffers(db_port_ind, count, sx_port_shared_buff_attr_arr);
        free(sx_port_shared_buff_attr_arr);
        free(sx_port_reserved_buff_attr_arr);
        SX_LOG_EXIT();
        return sai_status;
    }
    free(sx_port_shared_buff_attr_arr);
    free(sx_port_reserved_buff_attr_arr);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_apply_buffer_to_pg(_In_ uint32_t                           port_ind,
                                                       _In_ uint32_t                           pg_ind,
                                                       _In_ mlnx_sai_db_buffer_profile_entry_t buff_db_entry,
                                                       _In_ sai_object_id_t                    prev_pool)
{
    sai_status_t                  sai_status;
    mlnx_affect_port_buff_items_t affected_items;

    SX_LOG_ENTER();
    if (!alloc_affected_items(&affected_items)) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    affected_items.pgs[pg_ind]    = true;
    affected_items.affected_count = 1;
    sai_status                    = mlnx_sai_apply_buffer_settings_to_port(
        g_sai_db_ptr->ports_db[port_ind].logical,
        buff_db_entry,
        &affected_items,
        prev_pool);
    free_affected_items(&affected_items);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_buffer_apply_buffer_to_queue(_In_ uint32_t                           qos_db_port_ind,
                                                          _In_ uint32_t                           qos_ind,
                                                          _In_ mlnx_sai_db_buffer_profile_entry_t buff_db_entry,
                                                          _In_ sai_object_id_t                    prev_pool)
{
    sai_status_t                  sai_status;
    mlnx_affect_port_buff_items_t affected_items;

    if (!alloc_affected_items(&affected_items)) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    affected_items.tcs[qos_ind]   = true;
    affected_items.affected_count = 1;
    SX_LOG_ENTER();
    sai_status = mlnx_sai_apply_buffer_settings_to_port(g_sai_db_ptr->ports_db[qos_db_port_ind].logical,
                                                        buff_db_entry,
                                                        &affected_items,
                                                        prev_pool);
    free_affected_items(&affected_items);
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_collect_buffer_refs(_In_ sai_object_id_t                 sai_buffer_id,
                                                 _In_ uint32_t                        db_port_ind,
                                                 _Out_ mlnx_affect_port_buff_items_t* affected_items)
{
    uint32_t                    ind;
    sai_status_t                sai_status;
    uint32_t                    db_buffer_profile_index;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;
    bool                        found_match       = false;
    uint32_t                  * buff_profile_refs = NULL;
    mlnx_qos_queue_config_t   * queue_cfg         = NULL;
    uint32_t                    affected_count    = 0;

    SX_LOG_ENTER();

    if (NULL == affected_items) {
        SX_LOG_ERR("NULL affected_items\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (db_port_ind >= MAX_PORTS) {
        SX_LOG_ERR("Invalid db_port_ind:%d\n", db_port_ind);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_buffer_profile_data(sai_buffer_id, &db_buffer_profile_index, &sai_pool_attr))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_DBG("pool_type:%d\n", sai_pool_attr.pool_type);
    SX_LOG_DBG("db_port_ind:%d\n", db_port_ind);
    reset_affected_items(affected_items);

    switch (sai_pool_attr.pool_type) {
    case SAI_BUFFER_POOL_INGRESS:
        buff_profile_refs = NULL;
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_PG, &buff_profile_refs))) {
            SX_LOG_EXIT();
            return sai_status;
        }
        for (ind = 0; ind < buffer_limits.num_port_pg_buff; ind++) {
            if (buff_profile_refs[ind] == db_buffer_profile_index) {
                affected_items->pgs[ind] = true;
                SX_LOG_DBG("port[%d].pg[%d]\n", db_port_ind, ind);
                found_match = true;
                affected_count++;
            }
        }
        buff_profile_refs = NULL;
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_INGRESS, &buff_profile_refs))) {
            SX_LOG_EXIT();
            return sai_status;
        }
        for (ind = 0; ind < buffer_limits.num_ingress_pools; ind++) {
            if (buff_profile_refs[ind] == db_buffer_profile_index) {
                affected_items->i_port_buffers[ind] = true;
                found_match                         = true;
                affected_count++;
                SX_LOG_DBG("port[%d].i_buff[%d]\n", db_port_ind, ind);
            }
        }
        break;

    case SAI_BUFFER_POOL_EGRESS:
        buff_profile_refs = NULL;
        for (ind = 0; ind < buffer_limits.num_port_queue_buff; ind++) {
            if (SAI_STATUS_SUCCESS !=
                (sai_status =
                     mlnx_queue_cfg_lookup(g_sai_db_ptr->ports_db[db_port_ind].logical, ind, &queue_cfg))) {
                SX_LOG_EXIT();
                return sai_status;
            }
            if (queue_cfg->buffer_id == sai_buffer_id) {
                affected_items->tcs[ind] = true;
                found_match              = true;
                affected_count++;
                SX_LOG_DBG("port[%d].tc[%d]\n", db_port_ind, ind);
            }
        }
        buff_profile_refs = NULL;
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_EGRESS, &buff_profile_refs))) {
            SX_LOG_EXIT();
            return sai_status;
        }
        for (ind = 0; ind < buffer_limits.num_egress_pools; ind++) {
            if (buff_profile_refs[ind] == db_buffer_profile_index) {
                affected_items->e_port_buffers[ind] = true;
                found_match                         = true;
                affected_count++;
                SX_LOG_DBG("port[%d].e_buff[%d]\n", db_port_ind, ind);
            }
        }
        break;

    default:
        SX_LOG_ERR("Invalid pool type:%d\n", sai_pool_attr.pool_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_EXIT();
    if (!found_match) {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }
    affected_items->affected_count = affected_count;
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_apply_buffer_change_to_references(_In_ sai_object_id_t sai_buffer_id,
                                                                      _In_ sai_object_id_t prev_pool)
{
    sai_status_t                       sai_status;
    mlnx_affect_port_buff_items_t      affected_items;
    uint32_t                           port_ind;
    uint32_t                           db_buffer_profile_index;
    mlnx_sai_db_buffer_profile_entry_t buff_db_entry;
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr;
    mlnx_port_config_t                *port;

    SX_LOG_ENTER();
    if (!alloc_affected_items(&affected_items)) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_buffer_profile_data(sai_buffer_id, &db_buffer_profile_index, &sai_pool_attr))) {
        free_affected_items(&affected_items);
        SX_LOG_EXIT();
        return sai_status;
    }
    buff_db_entry = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index];
    mlnx_port_foreach(port, port_ind) {
        sai_status = mlnx_sai_collect_buffer_refs(sai_buffer_id, port_ind, &affected_items);
        if (SAI_STATUS_ITEM_NOT_FOUND == sai_status) {
            continue;
        }
        if (SAI_STATUS_SUCCESS != sai_status) {
            free_affected_items(&affected_items);
            SX_LOG_EXIT();
            return sai_status;
        }
        if (SAI_STATUS_SUCCESS != (sai_status =
                                       mlnx_sai_apply_buffer_settings_to_port(port->logical,
                                                                              buff_db_entry,
                                                                              &affected_items,
                                                                              prev_pool))) {
            free_affected_items(&affected_items);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    free_affected_items(&affected_items);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** reserved buffer size in bytes [sai_uint32_t] */
static sai_status_t mlnx_sai_set_buffer_profile_size_attr(_In_ const sai_object_key_t      * key,
                                                          _In_ const sai_attribute_value_t * value,
                                                          void                             * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].reserved_size = value->u32;
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, SAI_NULL_OBJECT_ID))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** dynamic threshold for the shared usage [sai_int8_t]
 * The threshold is set to the 2^n of available buffer of the pool.
 * Mandatory when SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC
 */
static sai_status_t mlnx_sai_get_buffer_profile_dynamic_th_attr(_In_ const sai_object_key_t   * key,
                                                                _Inout_ sai_attribute_value_t * value,
                                                                _In_ uint32_t                   attr_index,
                                                                _Inout_ vendor_cache_t        * cache,
                                                                void                          * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->s8 = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].shared_max.max.alpha;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** dynamic threshold for the shared usage [sai_int8_t]
 * The threshold is set to the 2^n of available buffer of the pool.
 * Mandatory when SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC
 */
static sai_status_t mlnx_sai_set_buffer_profile_dynamic_th_attr(_In_ const sai_object_key_t      * key,
                                                                _In_ const sai_attribute_value_t * value,
                                                                void                             * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].shared_max.max.alpha = value->s8;
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, SAI_NULL_OBJECT_ID))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** static threshold for the shared usage in bytes [sai_uint32_t]
 * Mandatory when SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_STATIC
 */
static sai_status_t mlnx_sai_get_buffer_profile_static_th_attr(_In_ const sai_object_key_t   * key,
                                                               _Inout_ sai_attribute_value_t * value,
                                                               _In_ uint32_t                   attr_index,
                                                               _Inout_ vendor_cache_t        * cache,
                                                               void                          * arg)
{
    SX_LOG_ENTER();
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->u32 = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].shared_max.max.static_th;
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** static threshold for the shared usage in bytes [sai_uint32_t]
 * Mandatory when SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_STATIC
 */
static sai_status_t mlnx_sai_set_buffer_profile_static_th_attr(_In_ const sai_object_key_t      * key,
                                                               _In_ const sai_attribute_value_t * value,
                                                               void                             * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].shared_max.max.static_th = value->u32;
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, SAI_NULL_OBJECT_ID))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** set the buffer profile XOFF threshold in bytes [sai_uint32_t]
 * Valid only for ingress PG.
 * Generate XOFF when available buffer in the PG buffer
 * is less than this threshold.
 * default to 0. */
static sai_status_t mlnx_sai_get_buffer_profile_xoff_attr(_In_ const sai_object_key_t   * key,
                                                          _Inout_ sai_attribute_value_t * value,
                                                          _In_ uint32_t                   attr_index,
                                                          _Inout_ vendor_cache_t        * cache,
                                                          void                          * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->u32 = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].xoff;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** set the buffer profile XOFF threshold in bytes [sai_uint32_t]
 * Valid only for ingress PG.
 * Generate XOFF when available buffer in the PG buffer
 * is less than this threshold.
 * default to 0. */
static sai_status_t mlnx_sai_set_buffer_profile_xoff_attr(_In_ const sai_object_key_t      * key,
                                                          _In_ const sai_attribute_value_t * value,
                                                          void                             * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].xoff = value->u32;
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, SAI_NULL_OBJECT_ID))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** set the buffer profile XON threshold in byte [sai_uint32_t]
 * Valid only for ingress PG
 * Generate XON when the total buffer usage of this PG
 * is less this threshold and available buffer in the PG buffer
 * is larger than the XOFF threahold.
 * default to 0. */
static sai_status_t mlnx_sai_get_buffer_profile_xon_attr(_In_ const sai_object_key_t   * key,
                                                         _Inout_ sai_attribute_value_t * value,
                                                         _In_ uint32_t                   attr_index,
                                                         _Inout_ vendor_cache_t        * cache,
                                                         void                          * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    value->u32 = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].xon;
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/** set the buffer profile XON threshold in byte [sai_uint32_t]
 * Valid only for ingress PG
 * Generate XON when the total buffer usage of this PG
 * is less this threshold and available buffer in the PG buffer
 * is larger than the XOFF threahold.
 * default to 0. */
static sai_status_t mlnx_sai_set_buffer_profile_xon_attr(_In_ const sai_object_key_t      * key,
                                                         _In_ const sai_attribute_value_t * value,
                                                         void                             * arg)
{
    sai_status_t sai_status;
    uint32_t     db_buffer_profile_index;

    SX_LOG_ENTER();
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = get_buffer_profile_db_index(key->object_id, &db_buffer_profile_index))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].xon = value->u32; /* TODO: unit conversion? */
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_buffer_apply_buffer_change_to_references(key->object_id, SAI_NULL_OBJECT_ID))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_sai_set_buffer_profile_attr(_In_ sai_object_id_t buffer_profile_id, _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .object_id = buffer_profile_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status;

    SX_LOG_ENTER();
    buffer_profile_key_to_str(buffer_profile_id, key_str);
    sai_status = sai_set_attribute(&key, key_str, buffer_profile_attribs, buffer_profile_vendor_attribs, attr);
    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t mlnx_sai_get_buffer_profile_attr(_In_ sai_object_id_t      buffer_profile_id,
                                              _In_ uint32_t             attr_count,
                                              _Inout_ sai_attribute_t * attr_list)
{
    char                   key_str[MAX_KEY_STR_LEN];
    const sai_object_key_t key = { .object_id = buffer_profile_id };
    sai_status_t           sai_status;

    SX_LOG_ENTER();
    buffer_profile_key_to_str(buffer_profile_id, key_str);
    if (SAI_NULL_OBJECT_ID == buffer_profile_id) {
        SX_LOG_ERR("NULL buffer profile passed in\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    sai_status = sai_get_attributes(
        &key,
        key_str,
        buffer_profile_attribs,
        buffer_profile_vendor_attribs,
        attr_count,
        attr_list);

    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t mlnx_sai_buffer_validate_port_buffer_list_and_sort_by_pool(_In_ const sai_attribute_value_t * value,
                                                                        _In_ bool                          is_ingress,
                                                                        _In_ uint32_t                      buff_count,
                                                                        _Out_ sai_object_id_t            * buffer_profiles)
{
    sai_status_t                sai_status;
    uint32_t                    ind;
    mlnx_sai_buffer_pool_attr_t sai_pool_attr;
    uint32_t                    db_buffer_profile_ind;
    uint32_t                    pool_base_ind;

    SX_LOG_ENTER();
    if (!value) {
        SX_LOG_ERR("null value\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (buff_count < value->objlist.count) {
        SX_LOG_ERR("buff_count:%d is smaller than input list size:%d\n", buff_count, value->objlist.count);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (is_ingress) {
        pool_base_ind = DEFAULT_INGRESS_SX_POOL_ID;
    } else {
        pool_base_ind = DEFAULT_EGRESS_SX_POOL_ID;
    }
    for (ind = 0; ind < value->objlist.count; ind++) {
        if (SAI_NULL_OBJECT_ID == value->objlist.list[ind]) {
            SX_LOG_ERR("NULL items not allowed in the list\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_get_sai_buffer_profile_data(value->objlist.list[ind], &db_buffer_profile_ind, &sai_pool_attr))) {
            SX_LOG_EXIT();
            return sai_status;
        }
        if (is_ingress) {
            if (SAI_BUFFER_POOL_EGRESS == sai_pool_attr.pool_type) {
                SX_LOG_ERR("Egress buffer profile:0x%" PRIx64 " specified for ingress port buffer list\n",
                           value->objlist.list[ind]);
                SX_LOG_EXIT();
                return SAI_STATUS_INVALID_PARAMETER;
            }
        } else {
            if (SAI_BUFFER_POOL_INGRESS == sai_pool_attr.pool_type) {
                SX_LOG_ERR("Ingress buffer profile:0x%" PRIx64 " specified for egress port buffer list\n",
                           value->objlist.list[ind]);
                SX_LOG_EXIT();
                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
        if (SAI_NULL_OBJECT_ID != buffer_profiles[sai_pool_attr.sx_pool_id - pool_base_ind]) {
            SX_LOG_ERR("SX pool:%d specified more than once\n", sai_pool_attr.sx_pool_id);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
        buffer_profiles[sai_pool_attr.sx_pool_id - pool_base_ind] = value->objlist.list[ind];
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_sai_buffer_apply_port_buffer_profile_list(_In_ bool             is_ingress,
                                                            _In_ uint32_t         db_port_ind,
                                                            _Inout_ uint32_t    * db_port_buffers,
                                                            _In_ uint32_t         buff_count,
                                                            _In_ sai_object_id_t* buffer_profiles)
{
    sai_status_t                       sai_status;
    uint32_t                           ind;
    mlnx_sai_db_buffer_profile_entry_t buffer_entry;
    sai_object_id_t                    sai_pool;
    uint32_t                           pool_id;
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr;
    mlnx_affect_port_buff_items_t      affected_items;
    uint32_t                           db_buffer_profile_ind;

    SX_LOG_ENTER();
    if (!alloc_affected_items(&affected_items)) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    for (ind = 0; ind < buff_count; ind++) {
        SX_LOG_DBG("buffer_profiles[%d]:0x%" PRIx64 "\n", ind, buffer_profiles[ind]);
        reset_affected_items(&affected_items);
        memset(&buffer_entry, 0, sizeof(buffer_entry));
        if (SAI_NULL_OBJECT_ID == buffer_profiles[ind]) {
            pool_id  = is_ingress ? DEFAULT_INGRESS_SX_POOL_ID : DEFAULT_EGRESS_SX_POOL_ID;
            pool_id += ind;
            if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_pool_id(pool_id, &sai_pool))) {
                free_affected_items(&affected_items);
                SX_LOG_EXIT();
                return sai_status;
            }
            if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(sai_pool, &sai_pool_attr))) {
                if (SAI_STATUS_ITEM_NOT_FOUND != sai_status) {
                    free_affected_items(&affected_items);
                    SX_LOG_EXIT();
                    return sai_status;
                }
                sai_status = SAI_STATUS_SUCCESS;
                SX_LOG_DBG("Skipping port pool buffer:%d, since the pool doesn't exist\n", ind);
                continue;
            }
            buffer_entry.is_valid = true;
            buffer_entry.sai_pool = sai_pool;

            SX_LOG_DBG("Resetting port buffer profile reference on port[%d].buff[%d], ingress:%d\n",
                       db_port_ind,
                       ind,
                       is_ingress);
            db_port_buffers[ind] = SENTINEL_BUFFER_DB_ENTRY_INDEX;
        } else {
            if (SAI_STATUS_SUCCESS !=
                (sai_status =
                     mlnx_get_sai_buffer_profile_data(buffer_profiles[ind], &db_buffer_profile_ind, &sai_pool_attr))) {
                free_affected_items(&affected_items);
                SX_LOG_EXIT();
                return sai_status;
            }
            buffer_entry         = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_ind];
            db_port_buffers[ind] = db_buffer_profile_ind;
        }
        if (is_ingress) {
            affected_items.i_port_buffers[ind] = true;
        } else {
            affected_items.e_port_buffers[ind] = true;
        }
        affected_items.affected_count = 1;
        /* Note: pool_id never changes for port pool buffer, hence last parameter == false */
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_apply_buffer_settings_to_port(
                                       g_sai_db_ptr->ports_db[db_port_ind].logical,
                                       buffer_entry,
                                       &affected_items,
                                       SAI_NULL_OBJECT_ID))) {
            free_affected_items(&affected_items);
            SX_LOG_EXIT();
            return sai_status;
        }
    }
    free_affected_items(&affected_items);
    SX_LOG_EXIT();
    return sai_status;
}

/*
 *  Passing in NULL items is not permitted in objlists.
 *  Usually for resetting a given port buffer item user would pass in NULL items, but since it's not allowed
 *  the workarounds are:
 *  - set corresponding buffer profile's values to 0 values.
 *  - replace (i.e. set another) buffer profile with the one with has 0 values.
 */
sai_status_t mlnx_buffer_port_profile_list_set(_In_ const sai_object_id_t         port_id,
                                               _In_ const sai_attribute_value_t * value,
                                               _In_ bool                          is_ingress)
{
    sai_status_t     sai_status;
    uint32_t         db_port_ind;
    uint32_t       * db_port_buffers = NULL;
    sai_object_id_t* buffer_profiles;
    uint32_t         buff_count;
    sx_port_log_id_t log_port;

    SX_LOG_ENTER();


    sai_status = mlnx_object_to_type(port_id, SAI_OBJECT_TYPE_PORT, &log_port, NULL);
    if (SAI_ERR(sai_status)) {
        return sai_status;
    }

    SX_LOG_DBG("is_ingress:%d\n", is_ingress);
    if (!value) {
        SX_LOG_ERR("NULL value\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (is_ingress) {
        if (value->objlist.count > buffer_limits.num_ingress_pools) {
            SX_LOG_ERR("Too many ingress entries specified\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    } else {
        if (value->objlist.count > buffer_limits.num_egress_pools) {
            SX_LOG_ERR("Too many egress entries specified\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_port_idx_by_log_id(log_port, &db_port_ind))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    if (is_ingress) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_INGRESS, &db_port_buffers))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            return sai_status;
        }
        buff_count = buffer_limits.num_ingress_pools;
    } else {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_EGRESS, &db_port_buffers))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            return sai_status;
        }
        buff_count = buffer_limits.num_egress_pools;
    }
    buffer_profiles = calloc(buff_count, sizeof(sai_object_id_t));
    if (NULL == buffer_profiles) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    if (SAI_STATUS_SUCCESS != (sai_status =
                                   mlnx_sai_buffer_validate_port_buffer_list_and_sort_by_pool(value, is_ingress,
                                                                                              buff_count,
                                                                                              buffer_profiles))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        free(buffer_profiles);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS == (sai_status =
                                   mlnx_sai_buffer_apply_port_buffer_profile_list(is_ingress,
                                                                                  db_port_ind,
                                                                                  db_port_buffers,
                                                                                  buff_count,
                                                                                  buffer_profiles))) {
        msync(g_sai_db_ptr, sizeof(*g_sai_db_ptr), MS_SYNC);
    }
    free(buffer_profiles);
    cl_plock_release(&g_sai_db_ptr->p_lock);
    SX_LOG_EXIT();
    return sai_status;
}


sai_status_t mlnx_buffer_port_profile_list_get(_In_ const sai_object_id_t      port_id,
                                               _Inout_ sai_attribute_value_t * value,
                                               _In_ bool                       is_ingress)
{
    sai_status_t     sai_status;
    uint32_t         db_port_ind;
    uint32_t         ind;
    sai_object_id_t* buffer_profiles;
    uint32_t         buff_count;
    uint32_t       * port_buff_profile_refs = NULL;
    uint32_t         ref_arr_count          = 0;
    uint32_t         list_cnt               = 0;
    sx_port_log_id_t log_port;

    SX_LOG_ENTER();

    sai_status = mlnx_object_to_type(port_id, SAI_OBJECT_TYPE_PORT, &log_port, NULL);
    if (SAI_ERR(sai_status)) {
        return sai_status;
    }

    buff_count      = max_value(buffer_limits.num_egress_pools, buffer_limits.num_ingress_pools);
    buffer_profiles = calloc(buff_count, sizeof(sai_object_id_t));
    if (NULL == buffer_profiles) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    sai_status = mlnx_port_idx_by_log_id(log_port, &db_port_ind);
    if (sai_status != SAI_STATUS_SUCCESS) {
        free(buffer_profiles);
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (is_ingress) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_INGRESS, &port_buff_profile_refs))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            free(buffer_profiles);
            SX_LOG_EXIT();
            return sai_status;
        }
        ref_arr_count = buffer_limits.num_ingress_pools;
    } else {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_sai_get_port_buffer_index_array(db_port_ind, PORT_BUFF_TYPE_EGRESS, &port_buff_profile_refs))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            free(buffer_profiles);
            SX_LOG_EXIT();
            return sai_status;
        }
        ref_arr_count = buffer_limits.num_egress_pools;
    }
    for (ind = 0; ind < ref_arr_count; ind++) {
        if (SENTINEL_BUFFER_DB_ENTRY_INDEX == port_buff_profile_refs[ind]) {
            continue;
        } else {
            SX_LOG_DBG("g_sai_db_ptr->ports_db[%d] %s port_buffers[%d]:%d\n", db_port_ind,
                       (is_ingress ? "ingress" : "egress"), ind, port_buff_profile_refs[ind]);
            if (SAI_STATUS_SUCCESS != (sai_status =
                                           mlnx_create_sai_buffer_profile_id(port_buff_profile_refs[ind],
                                                                             &buffer_profiles[list_cnt]))) {
                free(buffer_profiles);
                cl_plock_release(&g_sai_db_ptr->p_lock);
                SX_LOG_EXIT();
                return sai_status;
            }
            list_cnt++;
        }
    }
    cl_plock_release(&g_sai_db_ptr->p_lock);
    sai_status = mlnx_fill_objlist(buffer_profiles, list_cnt, &value->objlist);
    SX_LOG_EXIT();
    free(buffer_profiles);
    return sai_status;
}

sai_status_t mlnx_sai_get_buffer_pool_stats(_In_ sai_object_id_t                        pool_id,
                                            _In_ const sai_buffer_pool_stat_counter_t * counter_ids,
                                            _In_ uint32_t                               number_of_counters,
                                            _Out_ uint64_t                            * counters)
{
    sai_status_t                       sai_status;
    sx_cos_pool_occupancy_statistics_t occupancy_stats;
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr;
    char                               key_str[MAX_KEY_STR_LEN];
    uint32_t                           ii;

    SX_LOG_ENTER();
    pool_key_to_str(pool_id, key_str);
    SX_LOG_DBG("Get pool stats %s\n", key_str);
    if (0 == number_of_counters) {
        SX_LOG_ERR("0 number_of_counters array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == counter_ids) {
        SX_LOG_ERR("NULL counter ids array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == counters) {
        SX_LOG_ERR("NULL counters array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(pool_id, &sai_pool_attr))) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_release(&g_sai_db_ptr->p_lock);
    if (SX_STATUS_SUCCESS != (sai_status =
                                  sx_api_cos_pool_statistic_get(gh_sdk,
                                                                SX_ACCESS_CMD_READ,
                                                                &sai_pool_attr.sx_pool_id,
                                                                1,
                                                                &occupancy_stats))) {
        SX_LOG_ERR("Failed to get pool stat counters - error:%s.\n", SX_STATUS_MSG(sai_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sai_status);
    }
    for (ii = 0; ii < number_of_counters; ii++) {
        switch (counter_ids[ii]) {
        case SAI_BUFFER_POOL_STAT_CURR_OCCUPANCY_BYTES:
            counters[ii] = (uint64_t)mlnx_cells_to_bytes(occupancy_stats.statistics.curr_occupancy);
            break;

        case SAI_BUFFER_POOL_STAT_WATERMARK_BYTES:
            counters[ii] = (uint64_t)mlnx_cells_to_bytes(occupancy_stats.statistics.watermark);
            break;

        default:
            SX_LOG_ERR("Invalid counter id:%d\n", counter_ids[ii]);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_sai_get_ingress_priority_group_stats(
    _In_ sai_object_id_t                                   ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t * counter_ids,
    _In_ uint32_t                                          number_of_counters,
    _Out_ uint64_t                                       * counters)
{
    sai_status_t                     sai_status;
    uint32_t                         db_port_index, pg_ind;
    sx_port_statistic_usage_params_t stats_usage;
    sx_port_occupancy_statistics_t   occupancy_stats;
    uint32_t                         usage_cnt = 1;
    uint32_t                         ii;
    char                             key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();
    pg_key_to_str(ingress_pg_id, key_str);
    SX_LOG_DBG("Get PG stats %s\n", key_str);
    if (0 == number_of_counters) {
        SX_LOG_ERR("0 number_of_counters array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == counter_ids) {
        SX_LOG_ERR("NULL counter ids array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (NULL == counters) {
        SX_LOG_ERR("NULL counters array param\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = get_pg_data(ingress_pg_id, &db_port_index, &pg_ind))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    memset(&stats_usage, 0, sizeof(stats_usage));
    stats_usage.port_cnt                                 = 1;
    stats_usage.log_port_list_p                          = &g_sai_db_ptr->ports_db[db_port_index].logical;
    stats_usage.sx_port_params.port_params_type          = SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E;
    stats_usage.sx_port_params.port_params_cnt           = 1;
    stats_usage.sx_port_params.port_param.port_pg_list_p = &pg_ind;
    if (SX_STATUS_SUCCESS !=
        (sai_status = sx_api_cos_port_buff_type_statistic_get(gh_sdk, SX_ACCESS_CMD_READ, &stats_usage, 1,
                                                              &occupancy_stats, &usage_cnt))) {
        SX_LOG_ERR("Failed to get PG stat counters - %s.\n", SX_STATUS_MSG(sai_status));
        return sdk_to_sai(sai_status);
    }
    for (ii = 0; ii < number_of_counters; ii++) {
        switch (counter_ids[ii]) {
        case SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_SHARED_CURR_OCCUPANCY_BYTES:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_SHARED_CURR_OCCUPANCY_BYTES is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_SHARED_WATERMARK_BYTES:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_SHARED_WATERMARK_BYTES is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_XOFF_ROOM_CURR_OCCUPANCY_BYTES:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_XOFF_ROOM_CURR_OCCUPANCY_BYTES is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_XOFF_ROOM_WATERMARK_BYTES:
            SX_LOG_ERR("SAI_INGRESS_PRIORITY_GROUP_STAT_XOFF_ROOM_WATERMARK_BYTES is not supported\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_CURR_OCCUPANCY_BYTES:
            counters[ii] = (uint64_t)mlnx_cells_to_bytes(occupancy_stats.statistics.curr_occupancy);
            break;

        case SAI_INGRESS_PRIORITY_GROUP_STAT_WATERMARK_BYTES:
            counters[ii] = (uint64_t)mlnx_cells_to_bytes(occupancy_stats.statistics.watermark);
            break;

        default:
            SX_LOG_ERR("Invalid PG stat counter id:%d\n", counter_ids[ii]);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_clear_ingress_priority_group_stats(
    _In_ sai_object_id_t                                   ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t * counter_ids,
    _In_ uint32_t                                          number_of_counters)
{
    sai_status_t                     sai_status;
    sx_port_statistic_usage_params_t stats_usage;
    char                             key_str[MAX_KEY_STR_LEN];
    uint32_t                         db_port_index, pg_ind;
    sx_port_occupancy_statistics_t   occupancy_stats;
    uint32_t                         usage_cnt = 1;

    SX_LOG_ENTER();
    pg_key_to_str(ingress_pg_id, key_str);
    SX_LOG_NTC("Clear PG stats %s\n", key_str);

    if (SAI_STATUS_SUCCESS != (sai_status = get_pg_data(ingress_pg_id, &db_port_index, &pg_ind))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    memset(&stats_usage, 0, sizeof(stats_usage));
    stats_usage.port_cnt                                 = 1;
    stats_usage.log_port_list_p                          = &g_sai_db_ptr->ports_db[db_port_index].logical;
    stats_usage.sx_port_params.port_params_type          = SX_COS_INGRESS_PORT_PRIORITY_GROUP_ATTR_E;
    stats_usage.sx_port_params.port_params_cnt           = 1;
    stats_usage.sx_port_params.port_param.port_pg_list_p = &pg_ind;
    if (SX_STATUS_SUCCESS !=
        (sai_status = sx_api_cos_port_buff_type_statistic_get(gh_sdk, SX_ACCESS_CMD_READ_CLEAR, &stats_usage, 1,
                                                              &occupancy_stats, &usage_cnt))) {
        SX_LOG_ERR("Failed to clear PG stat counters - %s.\n", SX_STATUS_MSG(sai_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sai_status);
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_sai_buffer_init_pool_db()
{
    sai_status_t sai_status;

    SX_LOG_ENTER();
    /*  Egress CPU pool 7 always exists on startup, default SDK configuration does initialize it.
     *   Mark as allocated.
     */
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_set_e_cpu_pool_state(true))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_buffer_save_e_cpu_pool_initial_size())) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_buffer_apply(_In_ sai_object_id_t sai_buffer, _In_ sai_object_id_t to_obj_id)
{
    sai_status_t                       sai_status;
    uint32_t                           db_buffer_profile_index, prev_profile_db_index;
    uint32_t                           qos_db_port_index;
    sx_port_log_id_t                   logical_port_id;
    uint8_t                            qos_ext_data[EXTENDED_DATA_SIZE];
    mlnx_sai_buffer_pool_attr_t        sai_pool_attr, prev_sai_pool_attr;
    mlnx_sai_db_buffer_profile_entry_t buff_db_entry;
    sai_object_id_t                    sai_pool_4;
    mlnx_qos_queue_config_t          * queue_cfg = NULL;
    sai_object_id_t                    prev_pool = SAI_NULL_OBJECT_ID;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS != mlnx_object_to_type(to_obj_id, SAI_OBJECT_TYPE_QUEUE, &logical_port_id, qos_ext_data)) {
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_create_sai_pool_id(DEFAULT_EGRESS_SX_POOL_ID, &sai_pool_4))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    cl_plock_excl_acquire(&g_sai_db_ptr->p_lock);
    if (SAI_NULL_OBJECT_ID != sai_buffer) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_get_sai_buffer_profile_data(sai_buffer, &db_buffer_profile_index, &sai_pool_attr))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        buff_db_entry = g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index];
    } else {
        memset(&buff_db_entry, 0, sizeof(buff_db_entry));
        if (SAI_STATUS_SUCCESS != (sai_status = mlnx_get_sai_pool_data(sai_pool_4, &sai_pool_attr))) {
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return sai_status;
        }
        buff_db_entry.is_valid = true;
        buff_db_entry.sai_pool = sai_pool_4;
    }
    if (buffer_limits.num_port_queue_buff <= qos_ext_data[0]) {
        cl_plock_release(&g_sai_db_ptr->p_lock);
        SX_LOG_ERR("Queue object:0x%" PRIx64 ", refers to invalid queue index:%d\n", to_obj_id, qos_ext_data[0]);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (SAI_NULL_OBJECT_ID != sai_buffer) {
        if (SAI_BUFFER_POOL_EGRESS != sai_pool_attr.pool_type) {
            SX_LOG_ERR(
                "Buffer profile:0x%" PRIx64 " refers to INGRESS pool:0x%" PRIx64 ". Cannot be set on queue:0x%" PRIx64 "\n",
                sai_buffer,
                g_sai_buffer_db_ptr->buffer_profiles[db_buffer_profile_index].sai_pool,
                to_obj_id);
            cl_plock_release(&g_sai_db_ptr->p_lock);
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_PARAMETER;
        }
    }
    cl_plock_release(&g_sai_db_ptr->p_lock); /* unlock so mlnx_port_idx_by_log_id could obtain lock internally*/

    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_port_idx_by_log_id(logical_port_id, &qos_db_port_index))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    sai_db_write_lock();
    SX_LOG_DBG("Setting ports_db[%d].queue_list[%d].buffer_id=0x%" PRIx64 "\n",
               qos_db_port_index,
               qos_ext_data[0],
               sai_buffer);
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_queue_cfg_lookup(g_sai_db_ptr->ports_db[qos_db_port_index].logical, qos_ext_data[0],
                                   &queue_cfg))) {
        sai_db_unlock();
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_NULL_OBJECT_ID == queue_cfg->buffer_id) {
        if (sai_pool_4 != buff_db_entry.sai_pool) {
            prev_pool = sai_pool_4;
        }
    } else {
        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_get_sai_buffer_profile_data(queue_cfg->buffer_id, &prev_profile_db_index,
                                                  &prev_sai_pool_attr))) {
            sai_db_unlock();
            SX_LOG_EXIT();
            return sai_status;
        }
        if (g_sai_buffer_db_ptr->buffer_profiles[prev_profile_db_index].sai_pool != buff_db_entry.sai_pool) {
            prev_pool = g_sai_buffer_db_ptr->buffer_profiles[prev_profile_db_index].sai_pool;
        }
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_sai_buffer_apply_buffer_to_queue(qos_db_port_index, qos_ext_data[0], buff_db_entry, prev_pool))) {
        sai_db_unlock();
        SX_LOG_EXIT();
        return sai_status;
    }
    queue_cfg->buffer_id = sai_buffer;
    sai_qos_db_sync();
    sai_db_unlock();
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_delete_all_pools()
{
    sx_cos_pool_attr_t sx_pool_attr;
    sx_status_t        sx_status;
    uint32_t           pool_id = 0;

    SX_LOG_ENTER();
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    for (pool_id = 0; pool_id < buffer_limits.num_total_pools; pool_id++) {
        if (EGRESS_CPU_PORT_SX_POOL_ID == pool_id) {
            SX_LOG_DBG("Skipping deletion of pool#7 which is used for CPU port\n");
            continue;
        }
        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_cos_shared_buff_pool_set(gh_sdk, SX_ACCESS_CMD_DESTROY, &sx_pool_attr, &pool_id))) {
            SX_LOG_ERR("Failed deleting pool:%d. status:%d, message:%s. line:%d\n", pool_id, sx_status,
                       SX_STATUS_MSG(sx_status), __LINE__);
            SX_LOG_EXIT();
            return sdk_to_sai(sx_status);
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_reset_all_sx_pools()
{
    sx_cos_pool_attr_t sx_pool_attr;
    sx_status_t        sx_status;
    uint32_t           pool_id        = 0;
    uint32_t           busy_count     = 0;
    uint32_t           reset_pool_cnt = 0;

    SX_LOG_ENTER();
    memset(&sx_pool_attr, 0, sizeof(sx_cos_pool_attr_t));
    for (pool_id = 0; pool_id < buffer_limits.num_total_pools; pool_id++) {
        if (EGRESS_CPU_PORT_SX_POOL_ID == pool_id) {
            SX_LOG_DBG("Skipping resetting of pool#7 which is used for CPU port\n");
            continue;
        }
        sx_status = sx_api_cos_shared_buff_pool_get(gh_sdk, pool_id, &sx_pool_attr);
        if (SX_STATUS_ENTRY_NOT_FOUND == sx_status) {
            SX_LOG_NTC("Pool:%d not found\n", pool_id);
            continue;
        }
        if (SX_STATUS_SUCCESS != sx_status) {
            SX_LOG_ERR("Cannot get pool:%d data. reason:%d, %s. line:%d\n", pool_id, sx_status, SX_STATUS_MSG(
                           sx_status), __LINE__);
            SX_LOG_EXIT();
            return SAI_STATUS_FAILURE;
        }
        sx_pool_attr.pool_size = 0;
        sx_status              = sx_api_cos_shared_buff_pool_set(gh_sdk, SX_ACCESS_CMD_EDIT, &sx_pool_attr, &pool_id);
        SX_LOG_DBG("Result of pool reset. pool:%d data. status:%d, %s. line:%d\n", pool_id, sx_status,
                   SX_STATUS_MSG(sx_status), __LINE__);
        if (SX_STATUS_SUCCESS == sx_status) {
            reset_pool_cnt++;
        }
        if (SX_STATUS_ENTRY_ALREADY_BOUND == sx_status) {
            SX_LOG_NTC("pool:%d is busy\n", pool_id);
            busy_count++;
        }
    }
    SX_LOG_DBG("reset_pool_cnt:%d\n", reset_pool_cnt);
    SX_LOG_DBG("busy_count:%d\n", busy_count);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_unbind_reserved_buffers(_In_ sx_port_log_id_t log_port)
{
    sx_status_t                sx_status;
    uint32_t                   count = buffer_limits.max_buffers_per_port;
    sx_cos_port_buffer_attr_t* sx_port_reserved_buff_attr_arr;

    SX_LOG_ENTER();
    sx_port_reserved_buff_attr_arr = calloc(count, sizeof(sx_cos_port_buffer_attr_t));
    if (!sx_port_reserved_buff_attr_arr) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_buff_type_get(gh_sdk,
                                                                        log_port,
                                                                        sx_port_reserved_buff_attr_arr,
                                                                        &count))) {
        SX_LOG_ERR(
            "Failed to get number of bindings for reserved buffers. logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            log_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        free(sx_port_reserved_buff_attr_arr);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }

    sx_status = sx_api_cos_port_buff_type_set(gh_sdk,
                                              SX_ACCESS_CMD_DELETE,
                                              log_port,
                                              sx_port_reserved_buff_attr_arr,
                                              count);
    if (SX_STATUS_SUCCESS != sx_status) {
        SX_LOG_ERR(
            "Failed to delete bindings for reserved buffers. logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            log_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        free(sx_port_reserved_buff_attr_arr);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_DBG(
        "removed bindings for sx reserved buffers for logical:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
        log_port,
        count,
        sx_status,
        SX_STATUS_MSG(sx_status),
        __LINE__);
    free(sx_port_reserved_buff_attr_arr);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_unbind_shared_buffers(_In_ sx_port_log_id_t log_port)
{
    sx_status_t                       sx_status;
    sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr;
    uint32_t                          count = buffer_limits.max_buffers_per_port;

    SX_LOG_ENTER();
    sx_port_shared_buff_attr_arr = calloc(count, sizeof(sx_cos_port_shared_buffer_attr_t));
    if (!sx_port_shared_buff_attr_arr) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_cos_port_shared_buff_type_get(gh_sdk,
                                                                               log_port,
                                                                               sx_port_shared_buff_attr_arr,
                                                                               &count))) {
        SX_LOG_ERR("Failed to obtains sx shared buffers for logical:%d, sx_status:%d, message %s. line:%d\n",
                   log_port,  sx_status, SX_STATUS_MSG(sx_status), __LINE__);
        free(sx_port_shared_buff_attr_arr);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    sx_status = sx_api_cos_port_shared_buff_type_set(gh_sdk,
                                                     SX_ACCESS_CMD_DELETE,
                                                     log_port,
                                                     sx_port_shared_buff_attr_arr,
                                                     count);
    if (SX_STATUS_SUCCESS != sx_status) {
        SX_LOG_ERR(
            "Failed to delete bindings for shared buffers. logical:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            log_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        free(sx_port_shared_buff_attr_arr);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_DBG(
        "removed bindings for sx shared buffers for logical:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
        log_port,
        count,
        sx_status,
        SX_STATUS_MSG(sx_status),
        __LINE__);
    free(sx_port_shared_buff_attr_arr);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_delete_all_buffer_config()
{
    sai_status_t        sai_status;
    uint32_t            port_ind;
    mlnx_port_config_t *port;

    SX_LOG_ENTER();
    mlnx_port_foreach(port, port_ind) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_buffer_unbind_shared_buffers(port->logical))) {
            if (sai_status != SAI_STATUS_ITEM_NOT_FOUND) {
                SX_LOG_EXIT();
                return sai_status;
            }
        }
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_buffer_unbind_reserved_buffers(port->logical))) {
            if (sai_status != SAI_STATUS_ITEM_NOT_FOUND) {
                SX_LOG_EXIT();
                return sai_status;
            }
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}


static sai_status_t mlnx_sai_buffer_configure_reserved_buffers(_In_ sx_port_log_id_t            logical_port,
                                                               _Out_ sx_cos_port_buffer_attr_t* sx_port_reserved_buff_attr_arr,
                                                               _In_ uint32_t                    count)
{
    sx_status_t sx_status;

    SX_LOG_ENTER();

    sx_status = sx_api_cos_port_buff_type_set(gh_sdk,
                                              SX_ACCESS_CMD_SET,
                                              logical_port,
                                              sx_port_reserved_buff_attr_arr,
                                              count);
    if (SX_STATUS_SUCCESS != sx_status) {
        SX_LOG_ERR(
            "Failed to configure reserved buffers. logical port:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            logical_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_DBG(
        "Configured bindings for sx reserved buffers for logical port:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
        logical_port,
        count,
        sx_status,
        SX_STATUS_MSG(sx_status),
        __LINE__);

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_buffer_configure_shared_buffers(_In_ sx_port_log_id_t                   logical_port,
                                                             _Out_ sx_cos_port_shared_buffer_attr_t* sx_port_shared_buff_attr_arr,
                                                             _In_ uint32_t                           count)
{
    sx_status_t sx_status;

    SX_LOG_ENTER();
    sx_status = sx_api_cos_port_shared_buff_type_set(
        gh_sdk,
        SX_ACCESS_CMD_SET,
        logical_port,
        sx_port_shared_buff_attr_arr,
        count);
    if (SX_STATUS_SUCCESS != sx_status) {
        SX_LOG_ERR(
            "Failed to configure shared buffers. logical port:%d, number of items:%d sx_status:%d, message %s. line:%d\n",
            logical_port,
            count,
            sx_status,
            SX_STATUS_MSG(sx_status),
            __LINE__);
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }
    SX_LOG_DBG(
        "Configured bindings for sx shared buffers for logical port:%d, number of items:%d, sx_status:%d, message %s. line:%d\n",
        logical_port,
        count,
        sx_status,
        SX_STATUS_MSG(sx_status),
        __LINE__);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_is_buffer_in_use(_In_ sai_object_id_t buffer_profile_id)
{
    sai_status_t                  sai_status;
    uint32_t                      port_ind;
    mlnx_port_config_t           *port;
    mlnx_affect_port_buff_items_t affected_items;

    SX_LOG_ENTER();
    if (!alloc_affected_items(&affected_items)) {
        SX_LOG_EXIT();
        return SAI_STATUS_NO_MEMORY;
    }
    mlnx_port_foreach(port, port_ind) {
        sai_status = mlnx_sai_collect_buffer_refs(buffer_profile_id, port_ind, &affected_items);
        if (SAI_STATUS_ITEM_NOT_FOUND == sai_status) {
            continue;
        }
        if (SAI_STATUS_SUCCESS != sai_status) {
            SX_LOG_ERR("Failed to obtain references to buffer profile:0x%" PRIx64 "\n", buffer_profile_id);
            free_affected_items(&affected_items);
            SX_LOG_EXIT();
            return sai_status;
        }
        SX_LOG_ERR("Buffer profile is in use by port[%d].logical==%d\n",
                   port_ind, port->logical);
        log_buffer_profile_refs(&affected_items);
        free_affected_items(&affected_items);
        SX_LOG_EXIT();
        return SAI_STATUS_OBJECT_IN_USE;
    }
    free_affected_items(&affected_items);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_buffer_convert_alpha_sai_to_sx(_In_ sai_int8_t sai_alpha, _Out_ sx_cos_port_buff_alpha_e   *sx_alpha)
{
    SX_LOG_ENTER();
    if (NULL == sx_alpha) {
        SX_LOG_ERR("NULL sx_alpha\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (sai_alpha <= SAI_BUFFER_ALPHA_0) {
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_0_E;
        SX_LOG_DBG("input:%d, output:%d\n", sai_alpha, *sx_alpha);
        SX_LOG_EXIT();
        return SAI_STATUS_SUCCESS;
    }
    if (sai_alpha >= SAI_BUFFER_ALPHA_INFINITY) {
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_INFINITY_E;
        SX_LOG_DBG("input:%d, output:%d\n", sai_alpha, *sx_alpha);
        SX_LOG_EXIT();
        return SAI_STATUS_SUCCESS;
    }

    switch (sai_alpha) {
    case SAI_BUFFER_ALPHA_1_128:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_128_E;
        break;

    case SAI_BUFFER_ALPHA_1_64:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_64_E;
        break;

    case SAI_BUFFER_ALPHA_1_32:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_32_E;
        break;

    case SAI_BUFFER_ALPHA_1_16:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_16_E;
        break;

    case SAI_BUFFER_ALPHA_1_8:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_8_E;
        break;

    case SAI_BUFFER_ALPHA_1_4:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_4_E;
        break;

    case SAI_BUFFER_ALPHA_1_2:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_2_E;
        break;

    case SAI_BUFFER_ALPHA_1:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_1_E;
        break;

    case SAI_BUFFER_ALPHA_2:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_2_E;
        break;

    case SAI_BUFFER_ALPHA_4:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_4_E;
        break;

    case SAI_BUFFER_ALPHA_8:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_8_E;
        break;

    case SAI_BUFFER_ALPHA_16:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_16_E;
        break;

    case SAI_BUFFER_ALPHA_32:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_32_E;
        break;

    case SAI_BUFFER_ALPHA_64:
        *sx_alpha = SX_COS_PORT_BUFF_ALPHA_64_E;
        break;

    default:
        SX_LOG_ERR("Invalid sai alpha value:%d\n", sai_alpha);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    SX_LOG_DBG("input:%d, output:%d\n", sai_alpha, *sx_alpha);
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

const sai_buffer_api_t mlnx_buffer_api = {
    mlnx_sai_create_buffer_pool,
    mlnx_sai_remove_buffer_pool,
    mlnx_sai_set_buffer_pool_attr,
    mlnx_sai_get_buffer_pool_attr,
    mlnx_sai_get_buffer_pool_stats,
    mlnx_sai_set_ingress_priority_group_attr,
    mlnx_sai_get_ingress_priority_group_attr,
    mlnx_sai_get_ingress_priority_group_stats,
    mlnx_sai_clear_ingress_priority_group_stats,
    mlnx_sai_create_buffer_profile,
    mlnx_sai_remove_buffer_profile,
    mlnx_sai_set_buffer_profile_attr,
    mlnx_sai_get_buffer_profile_attr
};
