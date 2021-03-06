/*
 *  Copyright (C) 2014. Mellanox Technologies, Ltd. ALL RIGHTS RESERVED.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
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

#undef  __MODULE__
#define __MODULE__ SAI_TUNNEL

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;

/* mandatory_on_create, valid_for_create, valid_for_set, valid_for_get */
static const sai_attribute_entry_t tunnel_map_attribs[] = {
    { SAI_TUNNEL_MAP_ATTR_TYPE, true, true, false, true,
      "Tunnel map type", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST, false, true, false, true,
      "Tunnel map map to value list", SAI_ATTR_VAL_TYPE_TUNNELMAP },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, true,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};

/* mandatory_on_create, valid_for_create, valid_for_set, valid_for_get */
static const sai_attribute_entry_t tunnel_attribs[] = {
    { SAI_TUNNEL_ATTR_TYPE, true, true, false, true,
      "Tunnel type", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE, true, true, false, true,
      "Tunnel underlay interface", SAI_ATTR_VAL_TYPE_OID },
    { SAI_TUNNEL_ATTR_OVERLAY_INTERFACE, true, true, false, true,
      "Tunnel overlay interface", SAI_ATTR_VAL_TYPE_OID },
    { SAI_TUNNEL_ATTR_ENCAP_SRC_IP, false, true, true, true,
      "Tunnel encap src ip", SAI_ATTR_VAL_TYPE_IPADDR },
    { SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, false, true, false, true,
      "Tunnel encap ttl mode", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_ENCAP_TTL_VAL, false, true, true, true,
      "Tunnel encap ttl val", SAI_ATTR_VAL_TYPE_U8 },
    { SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE, false, true, false, true,
      "Tunnel encap dscp mode", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL, false, true, true, true,
      "Tunnel encap dstp val", SAI_ATTR_VAL_TYPE_U8 },
    { SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, false, true, false, true,
      "Tunnel encap gre key valid", SAI_ATTR_VAL_TYPE_BOOL },
    { SAI_TUNNEL_ATTR_ENCAP_GRE_KEY, false, true, false, true,
      "Tunnel encap gre key", SAI_ATTR_VAL_TYPE_U32 },
    { SAI_TUNNEL_ATTR_ENCAP_ECN_MODE, false, true, true, true,
      "Tunnel encap ecn mode", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_ENCAP_MAPPERS, false, true, true, true,
      "Tunnel encap mappers", SAI_ATTR_VAL_TYPE_OBJLIST },
    { SAI_TUNNEL_ATTR_DECAP_ECN_MODE, false, true, true, true,
      "Tunnel decap ecn mode", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_DECAP_MAPPERS, false, true, true, true,
      "Tunnel decap mappers", SAI_ATTR_VAL_TYPE_OBJLIST },
    { SAI_TUNNEL_ATTR_DECAP_TTL_MODE, false, true, false, true,
      "Tunnel decap ttl mode", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_ATTR_DECAP_DSCP_MODE, false, true, false, true,
      "Tunnel decap dscp mode", SAI_ATTR_VAL_TYPE_S32 },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, true,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};

/* mandatory_on_create, valid_for_create, valid_for_set, valid_for_get */
static const sai_attribute_entry_t tunnel_term_table_entry_attribs[] = {
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID, true, true, false, true,
      "Tunnel term vr id", SAI_ATTR_VAL_TYPE_OID },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, true, true, false, true,
      "Tunnel term type", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP, true, true, false, true,
      "Tunnel term dst ip", SAI_ATTR_VAL_TYPE_IPADDR },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP, false, true, false, true,
      "Tunnel term src ip", SAI_ATTR_VAL_TYPE_IPADDR },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE, true, true, false, true,
      "Tunnel term tunnel type", SAI_ATTR_VAL_TYPE_S32 },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID, true, true, false, true,
      "Tunnel term tunnel id", SAI_ATTR_VAL_TYPE_OID },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, true,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};
static sai_status_t mlnx_sai_get_tunnel_attribs(_In_ sai_object_id_t         sai_tunnel_id,
                                                _Out_ sx_tunnel_attribute_t *sx_tunnel_attr);
static sai_status_t mlnx_convert_sai_tunnel_type_to_sx(_In_ sai_tunnel_type_t  sai_type,
                                                       _Out_ sx_tunnel_type_e *sx_type);
static sai_status_t mlnx_convert_sx_tunnel_type_to_sai(_In_ sx_tunnel_type_e    sx_tunnel_attr,
                                                       _Out_ sai_tunnel_type_t *sai_type);
static sai_status_t mlnx_sai_tunnel_to_sx_tunnel_id(_In_ sai_object_id_t  sai_tunnel_id,
                                                    _Out_ sx_tunnel_id_t *sx_tunnel_id);
static sai_status_t mlnx_sai_get_sai_rif_id(_In_ sai_object_id_t        sai_tunnel_id,
                                            _In_ tunnel_rif_type        sai_tunnel_rif_type,
                                            _In_ sx_tunnel_attribute_t *sx_tunnel_attr,
                                            _Out_ sai_object_id_t      *sai_rif);
static sai_status_t mlnx_tunnel_map_attr_type_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg);
static sai_status_t mlnx_tunnel_map_attr_map_to_value_list_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg);
static sai_status_t mlnx_tunnel_type_get(_In_ const sai_object_key_t   *key,
                                         _Inout_ sai_attribute_value_t *value,
                                         _In_ uint32_t                  attr_index,
                                         _Inout_ vendor_cache_t        *cache,
                                         void                          *arg);
static sai_status_t mlnx_tunnel_rif_get(_In_ const sai_object_key_t   *key,
                                        _Inout_ sai_attribute_value_t *value,
                                        _In_ uint32_t                  attr_index,
                                        _Inout_ vendor_cache_t        *cache,
                                        void                          *arg);
static sai_status_t mlnx_tunnel_encap_src_ip_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg);
static sai_status_t mlnx_tunnel_ttl_mode_get(_In_ const sai_object_key_t   *key,
                                             _Inout_ sai_attribute_value_t *value,
                                             _In_ uint32_t                  attr_index,
                                             _Inout_ vendor_cache_t        *cache,
                                             void                          *arg);
static sai_status_t mlnx_tunnel_dscp_mode_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg);
static sai_status_t mlnx_tunnel_encap_gre_key_valid_get(_In_ const sai_object_key_t   *key,
                                                        _Inout_ sai_attribute_value_t *value,
                                                        _In_ uint32_t                  attr_index,
                                                        _Inout_ vendor_cache_t        *cache,
                                                        void                          *arg);
static sai_status_t mlnx_tunnel_encap_gre_key_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg);
static sai_status_t mlnx_tunnel_encap_ecn_mode_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg);
static sai_status_t mlnx_tunnel_decap_ecn_mode_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg);
static sai_status_t mlnx_tunnel_mappers_get(_In_ const sai_object_key_t   *key,
                                            _Inout_ sai_attribute_value_t *value,
                                            _In_ uint32_t                  attr_index,
                                            _Inout_ vendor_cache_t        *cache,
                                            void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_vr_id_get(_In_ const sai_object_key_t   *key,
                                                           _Inout_ sai_attribute_value_t *value,
                                                           _In_ uint32_t                  attr_index,
                                                           _Inout_ vendor_cache_t        *cache,
                                                           void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_type_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_dst_ip_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_src_ip_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_tunnel_type_get(_In_ const sai_object_key_t   *key,
                                                                 _Inout_ sai_attribute_value_t *value,
                                                                 _In_ uint32_t                  attr_index,
                                                                 _Inout_ vendor_cache_t        *cache,
                                                                 void                          *arg);
static sai_status_t mlnx_tunnel_term_table_entry_tunnel_id_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg);

/* is_implemented: create, remove, set, get
 *   is_supported: create, remove, set, get
 */
static const sai_vendor_attribute_entry_t tunnel_map_vendor_attribs[] = {
    { SAI_TUNNEL_MAP_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_map_attr_type_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_map_attr_map_to_value_list_get, NULL,
      NULL, NULL },
};

/* is_implemented: create, remove, set, get
 *   is_supported: create, remove, set, get
 */
static const sai_vendor_attribute_entry_t tunnel_vendor_attribs[] = {
    { SAI_TUNNEL_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_type_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_rif_get, (void*)MLNX_TUNNEL_UNDERLAY,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_OVERLAY_INTERFACE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_rif_get, (void*)MLNX_TUNNEL_OVERLAY,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_SRC_IP,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_encap_src_ip_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_TTL_MODE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_ttl_mode_get, (void*)TUNNEL_ENCAP,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_TTL_VAL,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_dscp_mode_get, (void*)TUNNEL_ENCAP,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_encap_gre_key_valid_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_GRE_KEY,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_encap_gre_key_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_ECN_MODE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_tunnel_encap_ecn_mode_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_ENCAP_MAPPERS,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_mappers_get, (void*)TUNNEL_ENCAP,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_DECAP_ECN_MODE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_tunnel_decap_ecn_mode_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_DECAP_MAPPERS,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_mappers_get, (void*)TUNNEL_DECAP,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_DECAP_TTL_MODE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_ttl_mode_get, (void*)TUNNEL_DECAP,
      NULL, NULL },
    { SAI_TUNNEL_ATTR_DECAP_DSCP_MODE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_dscp_mode_get, (void*)TUNNEL_DECAP,
      NULL, NULL },
};

/* is_implemented: create, remove, set, get
 *   is_supported: create, remove, set, get
 */
static const sai_vendor_attribute_entry_t tunnel_term_table_entry_vendor_attribs[] = {
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_vr_id_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_type_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_dst_ip_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_src_ip_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_tunnel_type_get, NULL,
      NULL, NULL },
    { SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_tunnel_term_table_entry_tunnel_id_get, NULL,
      NULL, NULL },
};
static void tunnel_map_key_to_str(_In_ const sai_object_id_t sai_tunnel_map_obj_id, _Out_ char *key_str)
{
    uint32_t internal_tunnel_map_obj_id = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        mlnx_object_to_type(sai_tunnel_map_obj_id, SAI_OBJECT_TYPE_TUNNEL_MAP, &internal_tunnel_map_obj_id, NULL)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "Invalid sai tunnel map obj ID %" PRIx64 "", sai_tunnel_map_obj_id);
    } else {
        snprintf(key_str,
                 MAX_KEY_STR_LEN,
                 "tunnel map ID %d",
                 internal_tunnel_map_obj_id);
    }

    SX_LOG_EXIT();
}

static void tunnel_key_to_str(_In_ const sai_object_id_t sai_tunnel_obj_id, _Out_ char *key_str)
{
    uint32_t internal_tunnel_obj_id = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        mlnx_object_to_type(sai_tunnel_obj_id, SAI_OBJECT_TYPE_TUNNEL, &internal_tunnel_obj_id, NULL)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "Invalid sai tunnel obj ID %" PRIx64 "", sai_tunnel_obj_id);
    } else {
        snprintf(key_str,
                 MAX_KEY_STR_LEN,
                 "tunnel ID %d",
                 internal_tunnel_obj_id);
    }

    SX_LOG_EXIT();
}

static void tunnel_term_table_entry_key_to_str(_In_ const sai_object_id_t sai_tunnel_term_table_entry_obj_id,
                                               _Out_ char                *key_str)
{
    uint32_t internal_tunnel_term_table_entry_obj_id = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        mlnx_object_to_type(sai_tunnel_term_table_entry_obj_id, SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
                            &internal_tunnel_term_table_entry_obj_id, NULL)) {
        snprintf(key_str,
                 MAX_KEY_STR_LEN,
                 "Invalid sai tunnel term table entry obj ID %" PRIx64 "",
                 sai_tunnel_term_table_entry_obj_id);
    } else {
        snprintf(key_str,
                 MAX_KEY_STR_LEN,
                 "tunnel term table entry ID %d",
                 internal_tunnel_term_table_entry_obj_id);
    }

    SX_LOG_EXIT();
}

/* caller needs to guard this function with lock */
static sai_status_t mlnx_get_sai_tunnel_map_db_idx(_In_ sai_object_id_t sai_tunnel_map_obj_id,
                                                   _Out_ uint32_t      *tunnel_mapper_db_idx)
{
    sai_status_t sai_status     = SAI_STATUS_FAILURE;
    uint32_t     tunnel_map_idx = 0;

    SX_LOG_ENTER();

    if (NULL == tunnel_mapper_db_idx) {
        SX_LOG_ERR("tunnel mapper db idx is null ptr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(sai_tunnel_map_obj_id, SAI_OBJECT_TYPE_TUNNEL_MAP, &tunnel_map_idx,
                                 NULL))) {
        SX_LOG_ERR("Invalid sai tunnel map obj id: %" PRIx64 "\n", sai_tunnel_map_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (MLNX_TUNNEL_MAP_MAX <= tunnel_map_idx) {
        SX_LOG_ERR("tunnel map idx %d is bigger than upper bound %d\n", tunnel_map_idx, MLNX_TUNNEL_MAP_MAX);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    if (!g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx].in_use) {
        SX_LOG_ERR("Non-exist tunnel map idx: %d\n", tunnel_map_idx);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    *tunnel_mapper_db_idx = tunnel_map_idx;

    SX_LOG_EXIT();
    return sai_status;
}

/* caller needs to guard the call with lock */
static sai_status_t mlnx_tunnel_map_db_param_get(_In_ const sai_object_id_t sai_tunnel_map_obj_id,
                                                 _Out_ mlnx_tunnel_map_t   *mlnx_tunnel_map)
{
    sai_status_t sai_status     = SAI_STATUS_FAILURE;
    uint32_t     tunnel_map_idx = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_get_sai_tunnel_map_db_idx(sai_tunnel_map_obj_id, &tunnel_map_idx))) {
        SX_LOG_ERR("Error getting tunnel mapper db idx from tunnel mapper obj id %" PRIx64 "\n",
                   sai_tunnel_map_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (!g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx].in_use) {
        SX_LOG_ERR("Non-exist tunnel map idx: %d\n", tunnel_map_idx);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    memcpy(mlnx_tunnel_map, &g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx], sizeof(mlnx_tunnel_map_t));

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_map_db_param_get_from_db(_In_ const sai_object_id_t sai_tunnel_map_obj_id,
                                                         _Out_ mlnx_tunnel_map_t   *mlnx_tunnel_map)
{
    sai_status_t sai_status = SAI_STATUS_FAILURE;

    assert(NULL != g_sai_db_ptr);

    sai_db_read_lock();

    sai_status = mlnx_tunnel_map_db_param_get(sai_tunnel_map_obj_id, mlnx_tunnel_map);

    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Fail to get mlnx tunnel map param for sai tunnel map obj id %" PRIx64 "\n", sai_tunnel_map_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    return sai_status;
}

static sai_status_t mlnx_tunnel_map_attr_type_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg)
{
    mlnx_tunnel_map_t mlnx_tunnel_map;
    sai_status_t      sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_map_db_param_get_from_db(key->object_id, &mlnx_tunnel_map))) {
        SX_LOG_ERR("Fail to get mlnx tunnel map for tunnel map obj id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    value->s32 = mlnx_tunnel_map.tunnel_map_type;

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_map_attr_map_to_value_list_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg)
{
    mlnx_tunnel_map_t mlnx_tunnel_map;
    sai_status_t      sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_map_db_param_get_from_db(key->object_id, &mlnx_tunnel_map))) {
        SX_LOG_ERR("Fail to get mlnx tunnel map for tunnel map obj id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_fill_tunnelmaplist(mlnx_tunnel_map.tunnel_map_list,
                                              mlnx_tunnel_map.tunnel_map_list_count,
                                              &value->tunnelmap))) {
        SX_LOG_ERR("fail to fill tunnel map list\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_type_get(_In_ const sai_object_key_t   *key,
                                         _Inout_ sai_attribute_value_t *value,
                                         _In_ uint32_t                  attr_index,
                                         _Inout_ vendor_cache_t        *cache,
                                         void                          *arg)
{
    sai_status_t          sai_status;
    sx_tunnel_attribute_t sx_tunnel_attr;
    sai_tunnel_type_t     sai_tunnel_type;

    SX_LOG_ENTER();
    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();
    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting tunnel attributes\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_convert_sx_tunnel_type_to_sai(sx_tunnel_attr.type, &sai_tunnel_type))) {
        SX_LOG_ERR("Error converting sx tunnel type to sai\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    value->s32 = sai_tunnel_type;

    SX_LOG_EXIT();
    return sai_status;
}

/* caller needs to guard this function with lock */
static sai_status_t mlnx_get_sai_tunnel_db_idx(_In_ sai_object_id_t sai_tunnel_id, _Out_ uint32_t      *tunnel_db_idx)
{
    sai_status_t sai_status;

    SX_LOG_ENTER();
    if (!tunnel_db_idx) {
        SX_LOG_ERR("NULL tunnel db idx\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(sai_tunnel_id, SAI_OBJECT_TYPE_TUNNEL, tunnel_db_idx, NULL))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (*tunnel_db_idx >= MAX_TUNNEL_DB_SIZE) {
        SX_LOG_ERR("tunnel db index:%d out of bounds:%d\n", *tunnel_db_idx, MAX_TUNNEL_DB_SIZE);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    if (!g_sai_db_ptr->tunnel_db[*tunnel_db_idx].is_used) {
        SX_LOG_ERR("tunnel db index:%d item marked as not used\n", *tunnel_db_idx);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    SX_LOG_EXIT();
    return sai_status;
}

/* caller needs to guard this function with lock */
static sai_status_t mlnx_get_tunnel_db_entry(_In_ sai_object_id_t     sai_tunnel_id,
                                             _Out_ tunnel_db_entry_t *sai_tunnel_db_entry)
{
    sai_status_t sai_status;
    uint32_t     tunnel_db_idx;

    SX_LOG_ENTER();

    if (NULL == sai_tunnel_db_entry) {
        SX_LOG_ERR("SAI tunnel db entry pointer is null\n");
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_tunnel_db_idx(sai_tunnel_id, &tunnel_db_idx))) {
        SX_LOG_ERR("Error getting sai tunnel db idx from sai tunnel id %" PRIx64 "\n", sai_tunnel_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    memcpy(sai_tunnel_db_entry, &g_sai_db_ptr->tunnel_db[tunnel_db_idx], sizeof(tunnel_db_entry_t));

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_get_sai_rif_id(_In_ sai_object_id_t        sai_tunnel_id,
                                            _In_ tunnel_rif_type        sai_tunnel_rif_type,
                                            _In_ sx_tunnel_attribute_t *sx_tunnel_attr,
                                            _Out_ sai_object_id_t      *sai_rif)
{
    sai_status_t          sai_status;
    sx_router_interface_t sdk_rif = 0;
    tunnel_db_entry_t     sai_tunnel_db_entry;

    SX_LOG_ENTER();

    switch (sx_tunnel_attr->type) {
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4:
        switch (sai_tunnel_rif_type) {
        case MLNX_TUNNEL_OVERLAY:
            sdk_rif = sx_tunnel_attr->attributes.ipinip_p2p.overlay_rif;
            break;

        case MLNX_TUNNEL_UNDERLAY:
            sdk_rif = sx_tunnel_attr->attributes.ipinip_p2p.underlay_rif;
            break;

        default:
            SX_LOG_ERR("Unrecognized sai tunnel rif type %d\n", sai_tunnel_rif_type);
            SX_LOG_EXIT();
            return SAI_STATUS_FAILURE;
            break;
        }
        break;

    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE:
        switch (sai_tunnel_rif_type) {
        case MLNX_TUNNEL_OVERLAY:
            sdk_rif = sx_tunnel_attr->attributes.ipinip_p2p_gre.overlay_rif;
            break;

        case MLNX_TUNNEL_UNDERLAY:
            sdk_rif = sx_tunnel_attr->attributes.ipinip_p2p_gre.underlay_rif;
            break;

        default:
            SX_LOG_ERR("Unrecognized sai tunnel rif type %d\n", sai_tunnel_rif_type);
            SX_LOG_EXIT();
            return SAI_STATUS_FAILURE;
            break;
        }
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN:
        sai_db_read_lock();
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_get_tunnel_db_entry(sai_tunnel_id,
                                                   &sai_tunnel_db_entry))) {
            SX_LOG_ERR("Failed to get tunnel db entry for sai tunnel id %" PRIx64 "\n", sai_tunnel_id);
            sai_db_unlock();
            SX_LOG_EXIT();
            return sai_status;
        }
        sai_db_unlock();
        switch (sai_tunnel_rif_type) {
        case MLNX_TUNNEL_OVERLAY:
            *sai_rif = sai_tunnel_db_entry.sai_vxlan_overlay_rif;
            break;

        case MLNX_TUNNEL_UNDERLAY:
            *sai_rif = sai_tunnel_db_entry.sai_vxlan_underlay_rif;
            break;

        default:
            SX_LOG_ERR("Unrecognized sai tunnel rif type %d\n", sai_tunnel_rif_type);
            SX_LOG_EXIT();
            return SAI_STATUS_FAILURE;
            break;
        }
        break;

    default:
        SX_LOG_ERR("Unsupported tunnel type:%d\n", sx_tunnel_attr->type);
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if ((SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4 == sx_tunnel_attr->type) ||
        ((SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE == sx_tunnel_attr->type))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_create_object(SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                                             sdk_rif,
                                             NULL,
                                             sai_rif))) {
            SX_LOG_ERR("Error getting sai rif object from sdk rif %d\n", sdk_rif);
            SX_LOG_EXIT();
            return sai_status;
        }
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_rif_get(_In_ const sai_object_key_t   *key,
                                        _Inout_ sai_attribute_value_t *value,
                                        _In_ uint32_t                  attr_index,
                                        _Inout_ vendor_cache_t        *cache,
                                        void                          *arg)
{
    sai_status_t          sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_attribute_t sx_tunnel_attr;

    SX_LOG_ENTER();

    assert((MLNX_TUNNEL_OVERLAY == (long)arg) || (MLNX_TUNNEL_UNDERLAY == (long)arg));

    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();
    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting tunnel attributes\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_get_sai_rif_id(key->object_id, (long)arg, &sx_tunnel_attr, &value->oid))) {
        SX_LOG_ERR("Error getting sai rif id\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_encap_src_ip_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg)
{
    sai_status_t          sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_attribute_t sx_tunnel_attr;
    sx_ip_addr_t          sx_ip_addr;

    SX_LOG_ENTER();

    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting sdk tunnel attributes from sai tunnel object %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    switch (sx_tunnel_attr.type) {
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4:
        memcpy(&sx_ip_addr, &sx_tunnel_attr.attributes.ipinip_p2p.encap.underlay_sip, sizeof(sx_ip_addr));
        break;

    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE:
        memcpy(&sx_ip_addr, &sx_tunnel_attr.attributes.ipinip_p2p_gre.encap.underlay_sip, sizeof(sx_ip_addr));
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN:
        memcpy(&sx_ip_addr, &sx_tunnel_attr.attributes.vxlan.encap.underlay_sip, sizeof(sx_ip_addr));
        break;

    default:
        SX_LOG_ERR("Unrecognized sx tunnel type: %d\n", sx_tunnel_attr.type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
        break;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_translate_sdk_ip_address_to_sai(&sx_ip_addr, &value->ipaddr))) {
        SX_LOG_ERR("Error translating sdk ip address to sai ip address\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_ttl_mode_get(_In_ const sai_object_key_t   *key,
                                             _Inout_ sai_attribute_value_t *value,
                                             _In_ uint32_t                  attr_index,
                                             _Inout_ vendor_cache_t        *cache,
                                             void                          *arg)
{
    SX_LOG_ENTER();

    value->s32 = SAI_TUNNEL_TTL_PIPE_MODEL;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_dscp_mode_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg)
{
    SX_LOG_ENTER();

    value->s32 = SAI_TUNNEL_DSCP_UNIFORM_MODEL;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_encap_gre_key_valid_get(_In_ const sai_object_key_t   *key,
                                                        _Inout_ sai_attribute_value_t *value,
                                                        _In_ uint32_t                  attr_index,
                                                        _Inout_ vendor_cache_t        *cache,
                                                        void                          *arg)
{
    sai_status_t          sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_attribute_t sx_tunnel_attr;

    SX_LOG_ENTER();

    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting sdk tunnel attributes from sai tunnel object %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE != sx_tunnel_attr.type) {
        SX_LOG_ERR("encap gre key valid is only valid for ip in ip gre type\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (sx_tunnel_attr.attributes.ipinip_p2p.encap.gre_mode) {
    case SX_TUNNEL_IPINIP_GRE_MODE_ENABLED_WITH_KEY:
        value->booldata = true;
        break;

    case SX_TUNNEL_IPINIP_GRE_MODE_ENABLED:
        value->booldata = false;
        break;

    default:
        SX_LOG_ERR("unrecognized sx tunnel encap gre mode %d\n", sx_tunnel_attr.attributes.ipinip_p2p.encap.gre_mode);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
        break;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_encap_gre_key_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg)
{
    sai_status_t          sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_attribute_t sx_tunnel_attr;

    SX_LOG_ENTER();

    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting sdk tunnel attributes from sai tunnel object %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE != sx_tunnel_attr.type) {
        SX_LOG_ERR("encap gre key is only valid for ip in ip gre type\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (sx_tunnel_attr.attributes.ipinip_p2p.encap.gre_mode) {
    case SX_TUNNEL_IPINIP_GRE_MODE_ENABLED_WITH_KEY:
        value->u32 = sx_tunnel_attr.attributes.ipinip_p2p.encap.gre_key;
        break;

    case SX_TUNNEL_IPINIP_GRE_MODE_ENABLED:
        SX_LOG_ERR("error: sx tunnel encap type is gre mode enabled without key\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
        break;

    default:
        SX_LOG_ERR("unrecognized sx tunnel encap gre mode %d\n", sx_tunnel_attr.attributes.ipinip_p2p.encap.gre_mode);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
        break;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_encap_ecn_mode_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg)
{
    SX_LOG_ENTER();

    value->s32 = SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_decap_ecn_mode_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg)
{
    SX_LOG_ENTER();

    value->s32 = SAI_TUNNEL_DECAP_ECN_MODE_STANDARD;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_ipinip_ecn_mapper_get(_Inout_ sai_attribute_value_t *value,
                                                      void                          *arg)
{
    SX_LOG_ENTER();

    SX_LOG_ERR("Tunnel mapper for ipinip/ipinip gre tunnel is not supported for get yet\n");

    value->objlist.count = 0;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_tunnel_vxlan_mapper_get(_In_ sai_object_id_t           sai_tunnel_obj_id,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 void                          *arg)
{
    sai_status_t sai_status        = SAI_STATUS_FAILURE;
    uint32_t     sai_tunnel_db_idx = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(sai_tunnel_obj_id, SAI_OBJECT_TYPE_TUNNEL, &sai_tunnel_db_idx, NULL))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    assert((TUNNEL_ENCAP == (long)arg) || (TUNNEL_DECAP == (long)arg));

    if (TUNNEL_ENCAP == (long)arg) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_fill_objlist(g_sai_db_ptr->tunnel_db[sai_tunnel_db_idx].sai_tunnel_map_encap_id_array,
                                            g_sai_db_ptr->tunnel_db[sai_tunnel_db_idx].sai_tunnel_map_encap_cnt,
                                            &value->objlist))) {
            SX_LOG_ERR("Error filling objlist for sai tunnel obj id %" PRId64 "\n", sai_tunnel_obj_id);
            goto cleanup;
        }
    } else if (TUNNEL_DECAP == (long)arg) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_fill_objlist(g_sai_db_ptr->tunnel_db[sai_tunnel_db_idx].sai_tunnel_map_decap_id_array,
                                            g_sai_db_ptr->tunnel_db[sai_tunnel_db_idx].sai_tunnel_map_decap_cnt,
                                            &value->objlist))) {
            SX_LOG_ERR("Error filling objlist for sai tunnel obj id %" PRId64 "\n", sai_tunnel_obj_id);
            goto cleanup;
        }
    }

    sai_status = SAI_STATUS_SUCCESS;

cleanup:
    SX_LOG_EXIT();

    return sai_status;
}

static sai_status_t mlnx_tunnel_mappers_get(_In_ const sai_object_key_t   *key,
                                            _Inout_ sai_attribute_value_t *value,
                                            _In_ uint32_t                  attr_index,
                                            _Inout_ vendor_cache_t        *cache,
                                            void                          *arg)
{
    sx_tunnel_attribute_t sx_tunnel_attr;
    sai_status_t          sai_status   = SAI_STATUS_FAILURE;
    sx_tunnel_id_t        sx_tunnel_id = 0;

    SX_LOG_ENTER();

    assert((TUNNEL_ENCAP == (long)arg) || (TUNNEL_DECAP == (long)arg));

    sai_db_write_lock();
    sai_status = mlnx_sai_get_tunnel_attribs(key->object_id, &sx_tunnel_attr);
    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error getting sdk tunnel attributes from sai tunnel object %" PRId64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sai_tunnel_to_sx_tunnel_id(key->object_id, &sx_tunnel_id))) {
        SX_LOG_ERR("Failed to get sx tunnel id form sai tunnel id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    switch (sx_tunnel_attr.type) {
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4:
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE:
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_tunnel_ipinip_ecn_mapper_get(value, arg))) {
            SX_LOG_ERR("Error getting ipinip ecn mapper\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN:
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_tunnel_vxlan_mapper_get(key->object_id, value, arg))) {
            SX_LOG_ERR("Error getting ipinip ecn mapper\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        break;

    default:
        SX_LOG_ERR("Unsupported sx tunnel type %d\n", sx_tunnel_attr.type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    sai_status = SAI_STATUS_SUCCESS;

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_convert_sai_tunnel_type_to_sx(_In_ sai_tunnel_type_t  sai_type,
                                                       _Out_ sx_tunnel_type_e *sx_type)
{
    SX_LOG_ENTER();
    switch (sai_type) {
    case SAI_TUNNEL_IPINIP:
        *sx_type = SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4;
        break;

    case SAI_TUNNEL_IPINIP_GRE:
        *sx_type = SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE;
        break;

    case SAI_TUNNEL_VXLAN:
        *sx_type = SX_TUNNEL_TYPE_NVE_VXLAN;
        break;

    default:
        SX_LOG_ERR("unsupported tunnel type:%d\n", sai_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_convert_sx_tunnel_type_to_sai(_In_ sx_tunnel_type_e    sx_tunnel_type,
                                                       _Out_ sai_tunnel_type_t *sai_type)
{
    SX_LOG_ENTER();
    switch (sx_tunnel_type) {
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4:
        *sai_type = SAI_TUNNEL_IPINIP;
        break;

    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE:
        *sai_type = SAI_TUNNEL_IPINIP_GRE;
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN:
        *sai_type = SAI_TUNNEL_VXLAN;
        break;

    default:
        SX_LOG_ERR("unsupported tunnel type:%d\n", sx_tunnel_type);
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_IMPLEMENTED;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_convert_sai_tunneltable_type_to_sx(_In_ sai_tunnel_term_table_entry_type_t  sai_type,
                                                            _Out_ sx_tunnel_decap_key_fields_type_e *sdk_type)
{
    SX_LOG_ENTER();
    switch (sai_type) {
    case SAI_TUNNEL_TERM_TABLE_ENTRY_P2P:
        *sdk_type = SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP_SIP;
        break;

    case SAI_TUNNEL_TERM_TABLE_ENTRY_P2MP:
        *sdk_type = SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP;
        break;

    default:
        SX_LOG_ERR("Unrecognized tunnel table type: %d\n", sai_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_convert_sx_tunneltable_type_to_sai(_In_ sx_tunnel_decap_key_fields_type_e    sdk_type,
                                                            _Out_ sai_tunnel_term_table_entry_type_t *sai_type)
{
    SX_LOG_ENTER();
    switch (sdk_type) {
    case SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP_SIP:
        *sai_type = SAI_TUNNEL_TERM_TABLE_ENTRY_P2P;
        break;

    case SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP:
        *sai_type = SAI_TUNNEL_TERM_TABLE_ENTRY_P2MP;
        break;

    default:
        SX_LOG_ERR("Unrecognized sdk tunnel decap key type %d\n", sdk_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_translate_sdk_tunnel_id_to_sai_tunnel_id(_In_ const sx_tunnel_id_t sdk_tunnel_id,
                                                           _Out_ sai_object_id_t    *sai_tunnel_id)
{
    sai_status_t sai_status = SAI_STATUS_FAILURE;
    uint32_t     tunnel_idx = 0;

    SX_LOG_ENTER();

    sai_db_read_lock();

    for (tunnel_idx = 0; tunnel_idx < MAX_TUNNEL_DB_SIZE; tunnel_idx++) {
        if (sdk_tunnel_id == g_sai_db_ptr->tunnel_db[tunnel_idx].sx_tunnel_id) {
            break;
        }
    }

    if (MAX_TUNNEL_DB_SIZE == tunnel_idx) {
        SX_LOG_ERR("Cannot find sai tunnel object which maps to sdk tunnel id %d\n", sdk_tunnel_id);
        sai_status = SAI_STATUS_FAILURE;
        goto cleanup;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL, tunnel_idx, NULL, sai_tunnel_id))) {
        SX_LOG_ERR("Cannot create sai tunnel object using index %d\n", tunnel_idx);
        goto cleanup;
    }

    sai_status = SAI_STATUS_SUCCESS;
cleanup:
    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

/* caller needs to guard the call with lock */
static sai_status_t mlnx_tunnel_term_table_entry_sdk_param_get(
    _In_ const sai_object_id_t         sai_tunneltable_obj_id,
    _Out_ sx_tunnel_decap_entry_key_t *sdk_tunnel_decap_key)
{
    sai_status_t sai_status                     = SAI_STATUS_FAILURE;
    uint32_t     internal_tunneltable_entry_idx = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(sai_tunneltable_obj_id, SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
                                 &internal_tunneltable_entry_idx,
                                 NULL))) {
        SX_LOG_ERR("Invalid sai tunnel table entry obj id: %" PRIx64 "\n", sai_tunneltable_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (MLNX_TUNNELTABLE_SIZE <= internal_tunneltable_entry_idx) {
        SX_LOG_ERR("Internal tunnel table entry idx %d is bigger than upper bound %d\n",
                   internal_tunneltable_entry_idx,
                   MLNX_TUNNELTABLE_SIZE);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    if (!g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_entry_idx].in_use) {
        SX_LOG_ERR("Non-exist internal tunnel table entry idx: %d\n", internal_tunneltable_entry_idx);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    memcpy(sdk_tunnel_decap_key,
           &g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_entry_idx].sdk_tunnel_decap_key,
           sizeof(sx_tunnel_decap_entry_key_t));

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_sdk_param_get_from_db(
    _In_ const sai_object_id_t         sai_tunneltable_obj_id,
    _Out_ sx_tunnel_decap_entry_key_t *sdk_tunnel_decap_key)
{
    sai_status_t sai_status = SAI_STATUS_FAILURE;

    sai_db_read_lock();

    sai_status = mlnx_tunnel_term_table_entry_sdk_param_get(sai_tunneltable_obj_id, sdk_tunnel_decap_key);

    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", sai_tunneltable_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_vr_id_get(_In_ const sai_object_key_t   *key,
                                                           _Inout_ sai_attribute_value_t *value,
                                                           _In_ uint32_t                  attr_index,
                                                           _Inout_ vendor_cache_t        *cache,
                                                           void                          *arg)
{
    sai_status_t                sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_decap_entry_key_t sdk_tunnel_decap_key;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_create_object(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sdk_tunnel_decap_key.underlay_vrid, NULL,
                                &value->oid))) {
        SX_LOG_ERR("Fail to get sai virtual router id from sdk underlay vrid %d\n",
                   sdk_tunnel_decap_key.underlay_vrid);
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_type_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg)
{
    sai_status_t                       sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_decap_entry_key_t        sdk_tunnel_decap_key;
    sai_tunnel_term_table_entry_type_t sai_tunneltable_entry_type;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_convert_sx_tunneltable_type_to_sai(sdk_tunnel_decap_key.type, &sai_tunneltable_entry_type))) {
        SX_LOG_ERR("Error converting sdk tunnel table entry type %d to sai tunnel table entry type\n",
                   sdk_tunnel_decap_key.type);
        SX_LOG_EXIT();
        return sai_status;
    }

    value->s32 = sai_tunneltable_entry_type;

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_dst_ip_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg)
{
    sai_status_t                sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_decap_entry_key_t sdk_tunnel_decap_key;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_translate_sdk_ip_address_to_sai(&sdk_tunnel_decap_key.underlay_dip,
                                                           &value->ipaddr))) {
        SX_LOG_ERR("Error getting dst ip of sai tunnel table entry id: %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_src_ip_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg)
{
    sai_status_t                sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_decap_entry_key_t sdk_tunnel_decap_key;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP_SIP != sdk_tunnel_decap_key.type) {
        SX_LOG_ERR(
            "src ip should not be got when tunnel table entry type is not P2P, here sdk tunnel decap key type is %d\n",
            sdk_tunnel_decap_key.type);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_translate_sdk_ip_address_to_sai(&sdk_tunnel_decap_key.underlay_sip,
                                                           &value->ipaddr))) {
        SX_LOG_ERR("Error getting src ip of sai tunnel table entry id: %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_tunnel_type_get(_In_ const sai_object_key_t   *key,
                                                                 _Inout_ sai_attribute_value_t *value,
                                                                 _In_ uint32_t                  attr_index,
                                                                 _Inout_ vendor_cache_t        *cache,
                                                                 void                          *arg)
{
    sai_status_t                sai_status = SAI_STATUS_FAILURE;
    sx_tunnel_decap_entry_key_t sdk_tunnel_decap_key;
    sai_tunnel_type_t           sai_tunnel_type;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_convert_sx_tunnel_type_to_sai(sdk_tunnel_decap_key.tunnel_type, &sai_tunnel_type))) {
        SX_LOG_ERR("Unrecognized sdk tunnel decap key tunnel type %d\n", sdk_tunnel_decap_key.tunnel_type);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    value->s32 = sai_tunnel_type;

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_tunnel_term_table_entry_tunnel_id_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg)
{
    sai_status_t                 sai_status = SAI_STATUS_FAILURE;
    sx_status_t                  sdk_status = SX_STATUS_ERROR;
    sx_tunnel_decap_entry_key_t  sdk_tunnel_decap_key;
    sx_tunnel_decap_entry_data_t sdk_tunnel_decap_data;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_tunnel_term_table_entry_sdk_param_get_from_db(key->object_id, &sdk_tunnel_decap_key))) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n", key->object_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_STATUS_SUCCESS !=
        (sdk_status = sx_api_tunnel_decap_rules_get(gh_sdk, &sdk_tunnel_decap_key, &sdk_tunnel_decap_data))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error getting tunnel id from sai tunnel table entry id %" PRIx64 ", sx status %s\n",
                   key->object_id,
                   SX_STATUS_MSG(sdk_status));
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_translate_sdk_tunnel_id_to_sai_tunnel_id(sdk_tunnel_decap_data.tunnel_id, &value->oid))) {
        SX_LOG_ERR("Error creating sai tunnel id from internal tunnel id %d\n", sdk_tunnel_decap_data.tunnel_id);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    SX_LOG_EXIT();
    return sai_status;
}

/* caller of this function should use read lock to guard the callsite */
static sai_status_t mlnx_create_empty_tunnel_map(_Out_ uint32_t *tunnel_map_idx)
{
    uint32_t     idx        = 0;
    sai_status_t sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    for (idx = MLNX_TUNNEL_MAP_MIN; idx < MLNX_TUNNEL_MAP_MAX; idx++) {
        if (!g_sai_db_ptr->mlnx_tunnel_map[idx].in_use) {
            *tunnel_map_idx = idx;
            sai_status      = SAI_STATUS_SUCCESS;
            goto cleanup;
        }
    }

    SX_LOG_ERR(
        "Not enough resources for sai tunnel map, at most %d sai tunnel map objs can be created\n",
        MLNX_TUNNEL_MAP_MAX);
    sai_status = SAI_STATUS_INSUFFICIENT_RESOURCES;

cleanup:
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_init_tunnel_map_param(_In_ uint32_t               attr_count,
                                               _In_ const sai_attribute_t *attr_list,
                                               _Out_ mlnx_tunnel_map_t    *mlnx_tunnel_map)
{
    const sai_attribute_value_t *tunnel_map_type = NULL, *tunnel_map_list = NULL;
    uint32_t                     attr_idx        = 0;
    sai_status_t                 sai_status      = SAI_STATUS_FAILURE;

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_MAP_ATTR_TYPE, &tunnel_map_type, &attr_idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    mlnx_tunnel_map->tunnel_map_type = tunnel_map_type->s32;

    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST, &tunnel_map_list,
                                 &attr_idx))) {
        if (tunnel_map_list->tunnelmap.count > MLNX_TUNNEL_MAP_LIST_MAX) {
            SX_LOG_ERR("Tunnel map overflow: size %u is greater than maxium size %u\n",
                       tunnel_map_list->tunnelmap.count, MLNX_TUNNEL_MAP_LIST_MAX);
            SX_LOG_EXIT();
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        mlnx_tunnel_map->tunnel_map_list_count = tunnel_map_list->tunnelmap.count;
        memcpy(mlnx_tunnel_map->tunnel_map_list, tunnel_map_list->tunnelmap.list,
               tunnel_map_list->tunnelmap.count * sizeof(sai_tunnel_map_t));
    }

    mlnx_tunnel_map->tunnel_cnt = 0;

    mlnx_tunnel_map->in_use = true;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_create_tunnel_map(_Out_ sai_object_id_t      *sai_tunnel_map_obj_id,
                                           _In_ uint32_t               attr_count,
                                           _In_ const sai_attribute_t *attr_list)
{
    char              list_str[MAX_LIST_VALUE_STR_LEN];
    sai_status_t      sai_status     = SAI_STATUS_SUCCESS;
    uint32_t          tunnel_map_idx = 0;
    mlnx_tunnel_map_t mlnx_tunnel_map;

    memset(&mlnx_tunnel_map, 0, sizeof(mlnx_tunnel_map_t));

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = check_attribs_metadata(attr_count, attr_list, tunnel_map_attribs, tunnel_map_vendor_attribs,
                                             SAI_COMMON_API_CREATE))) {
        SX_LOG_ERR("Tunnel map: metadata check failed\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_attr_list_to_str(attr_count, attr_list, tunnel_map_attribs, MAX_LIST_VALUE_STR_LEN, list_str);
    SX_LOG_NTC("SAI Tunnel map attributes: %s\n", list_str);

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_init_tunnel_map_param(attr_count, attr_list, &mlnx_tunnel_map))) {
        SX_LOG_ERR("Fail to set tunnel map param on create\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_db_write_lock();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_create_empty_tunnel_map(&tunnel_map_idx))) {
        SX_LOG_ERR("Failed to create empty tunnel map\n");
        goto cleanup;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL_MAP, tunnel_map_idx, NULL,
                                sai_tunnel_map_obj_id))) {
        memset(&g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx], 0,
               sizeof(mlnx_tunnel_map_t));
        SX_LOG_ERR("Error creating sai tunnel map obj id from tunnel map idx %d\n",
                   tunnel_map_idx);
        goto cleanup;
    }

    memcpy(&g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx], &mlnx_tunnel_map, sizeof(mlnx_tunnel_map_t));

    SX_LOG_NTC("Created SAI tunnel map obj id: %" PRIx64 "\n", *sai_tunnel_map_obj_id);

    sai_status = SAI_STATUS_SUCCESS;

cleanup:
    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_remove_tunnel_map(_In_ const sai_object_id_t sai_tunnel_map_obj_id)
{
    sai_status_t sai_status     = SAI_STATUS_FAILURE;
    uint32_t     tunnel_map_idx = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(sai_tunnel_map_obj_id, SAI_OBJECT_TYPE_TUNNEL_MAP, &tunnel_map_idx, NULL))) {
        SX_LOG_ERR("Invalid sai tunnel map obj id: %" PRIx64 "\n", sai_tunnel_map_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (MLNX_TUNNEL_MAP_MAX <= tunnel_map_idx) {
        SX_LOG_ERR("tunnel map idx %d is bigger than upper bound %d\n", tunnel_map_idx, MLNX_TUNNEL_MAP_MAX);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    sai_db_write_lock();

    if (g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx].in_use) {
        if (0 < g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx].tunnel_cnt) {
            SX_LOG_ERR("This tunnel map is still used by %d other tunnel(s)\n",
                       g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx].tunnel_cnt);
            sai_status = SAI_STATUS_OBJECT_IN_USE;
            goto cleanup;
        }
        memset(&g_sai_db_ptr->mlnx_tunnel_map[tunnel_map_idx], 0, sizeof(mlnx_tunnel_map_t));
    } else {
        SX_LOG_ERR("Invalid sai tunnel map obj id: %" PRIx64 "\n", sai_tunnel_map_obj_id);
        sai_status = SAI_STATUS_INVALID_OBJECT_ID;
        goto cleanup;
    }

    SX_LOG_NTC("Removed SAI tunnel map obj id %" PRIx64 "\n", sai_tunnel_map_obj_id);

    sai_status = SAI_STATUS_SUCCESS;

cleanup:
    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

/*
 *  Callers need to lock around this method
 */
static sai_status_t mlnx_sai_tunnel_to_sx_tunnel_id(_In_ sai_object_id_t  sai_tunnel_id,
                                                    _Out_ sx_tunnel_id_t *sx_tunnel_id)
{
    sai_status_t sai_status;
    uint32_t     tunnel_db_idx;

    SX_LOG_ENTER();
    if (!sx_tunnel_id) {
        SX_LOG_ERR("NULL sx_tunnel_id\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_tunnel_db_idx(sai_tunnel_id, &tunnel_db_idx))) {
        SX_LOG_ERR("Error getting sai tunnel db idx from sai tunnel id %" PRIx64 "\n", sai_tunnel_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    *sx_tunnel_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sx_tunnel_id;
    SX_LOG_DBG("sx_tunnel_id:%d\n", *sx_tunnel_id);
    SX_LOG_EXIT();
    return sai_status;
}

/*
 *  Callers need to lock around this method
 */
static sai_status_t mlnx_sai_get_tunnel_attribs(_In_ sai_object_id_t         sai_tunnel_id,
                                                _Out_ sx_tunnel_attribute_t *sx_tunnel_attr)
{
    sai_status_t   sai_status;
    sx_status_t    sx_status;
    sx_tunnel_id_t sx_tunnel_id;

    SX_LOG_ENTER();

    if (!sx_tunnel_attr) {
        SX_LOG_ERR("NULL sx_tunnel_attr\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_tunnel_to_sx_tunnel_id(sai_tunnel_id, &sx_tunnel_id))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SX_STATUS_SUCCESS != (sx_status = sx_api_tunnel_get(gh_sdk, sx_tunnel_id, sx_tunnel_attr))) {
        sai_status = sdk_to_sai(sx_status);
        SX_LOG_ERR("Error getting sx tunnel for sx tunnel id %d, sx status: %s\n", sx_tunnel_id,
                   SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return sai_status;
}

/*
 *  Callers need to lock around this method
 */
static sai_status_t mlnx_sai_reserve_tunnel_db_item(_In_ sx_tunnel_id_t sx_tunnel_id,
                                                    _Out_ uint32_t     *tunnel_db_idx)
{
    uint32_t ii;

    SX_LOG_ENTER();
    if (!tunnel_db_idx) {
        SX_LOG_ERR("NULL tunnel_db_idx\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    for (ii = 0; ii < MAX_TUNNEL_DB_SIZE; ii++) {
        if (!g_sai_db_ptr->tunnel_db[ii].is_used) {
            g_sai_db_ptr->tunnel_db[ii].is_used      = true;
            g_sai_db_ptr->tunnel_db[ii].sx_tunnel_id = sx_tunnel_id;
            *tunnel_db_idx                           = ii;
            SX_LOG_DBG("tunnel db: reserved slot:%d, sx_tunnel_id:%d\n", ii, sx_tunnel_id);
            SX_LOG_EXIT();
            return SAI_STATUS_SUCCESS;
        }
    }
    SX_LOG_EXIT();
    return SAI_STATUS_TABLE_FULL;
}

/*
 *  Callers need to lock around this method
 */
static sai_status_t mlnx_sai_tunnel_create_tunnel_object_id(_In_ sx_tunnel_id_t    sx_tunnel_id,
                                                            _Out_ sai_object_id_t *sai_tunnel_id)
{
    sai_status_t sai_status;
    uint32_t     tunnel_db_idx;

    SX_LOG_ENTER();
    if (!sai_tunnel_id) {
        SX_LOG_ERR("NULL sai_tunnel_id\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_sai_reserve_tunnel_db_item(sx_tunnel_id, &tunnel_db_idx))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL, tunnel_db_idx, NULL, sai_tunnel_id))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_sai_get_sx_vrid_from_sx_rif(_In_ sx_router_interface_t sdk_rif_id,
                                                     _Out_ sx_router_id_t      *sdk_vrid)
{
    sai_status_t                sai_status = SAI_STATUS_FAILURE;
    sx_status_t                 sdk_status = SX_STATUS_ERROR;
    sx_router_interface_param_t sdk_intf_params;
    sx_interface_attributes_t   sdk_intf_attribs;

    SX_LOG_ENTER();

    if (SX_STATUS_SUCCESS !=
        (sdk_status =
             sx_api_router_interface_get(gh_sdk, sdk_rif_id, sdk_vrid, &sdk_intf_params, &sdk_intf_attribs))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error getting sdk vrid from sdk rif id %d, sx status: %s\n", sdk_rif_id,
                   SX_STATUS_MSG(sdk_status));
        SX_LOG_EXIT();
        return sai_status;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sdk_fill_ipinip_p2p_attrib(_In_ uint32_t                           attr_count,
                                                    _In_ const sai_attribute_t             *attr_list,
                                                    _In_ sai_tunnel_type_t                  sai_tunnel_type,
                                                    _Out_ sx_tunnel_ipinip_p2p_attribute_t *sdk_ipinip_p2p_attrib,
                                                    _Out_ bool                             *has_encap_attr,
                                                    _Out_ bool                             *has_decap_attr)
{
    sai_status_t                 sai_status = SAI_STATUS_FAILURE;
    uint32_t                     data;
    const sai_attribute_value_t *attr;
    uint32_t                     attr_idx;
    sx_router_id_t               sdk_vrid;

    SX_LOG_ENTER();
    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_OVERLAY_INTERFACE, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_ROUTER_INTERFACE, &data, NULL))) {
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }
        sdk_ipinip_p2p_attrib->overlay_rif = (sx_router_interface_t)data;
    } else {
        SX_LOG_ERR("overlay interface should be specified on creating ip in ip type tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_ROUTER_INTERFACE, &data, NULL))) {
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }
        sdk_ipinip_p2p_attrib->underlay_rif = (sx_router_interface_t)data;

        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_get_sx_vrid_from_sx_rif((sx_router_interface_t)data, &sdk_vrid))) {
            SX_LOG_ERR("mlnx_sai_get_sx_vrid_from_sx_rif failed\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }

        sdk_ipinip_p2p_attrib->encap.underlay_vrid = sdk_vrid;
    } else {
        SX_LOG_ERR("underlay interface should be specified on creating ip in ip type tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_SRC_IP, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_translate_sai_ip_address_to_sdk(&attr->ipaddr,
                                                               &sdk_ipinip_p2p_attrib->encap.underlay_sip))) {
            SX_LOG_ERR("Error setting src ip on creating tunnel table\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        *has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_TTL_PIPE_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_TTL_PIPE_MODEL is supported for tunnel encap\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        *has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_DSCP_UNIFORM_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_DSCP_COPY_FROM_INNER is supported for tunnel encap\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        *has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_TTL_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR(
            "Failed to obtain required attribute SAI_TUNNEL_ATTR_DECAP_TTL_MODE for SAI_TUNNEL_IPINIP or SAI_TUNNEL_IPINIP_GRE tunnel type\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_TUNNEL_TTL_PIPE_MODEL != attr->s32) {
        SX_LOG_ERR("Only SAI_TUNNEL_TTL_PIPE_MODEL is supported for tunnel decap\n");
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_SUPPORTED;
    }
    *has_decap_attr = true;

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_DSCP_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR(
            "Failed to obtain required attribute SAI_TUNNEL_ATTR_DECAP_DSCP_MODE for SAI_TUNNEL_IPINIP or SAI_TUNNEL_IPINIP_GRE tunnel type\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SAI_TUNNEL_DSCP_UNIFORM_MODEL != attr->s32) {
        SX_LOG_ERR("Only SAI_TUNNEL_DSCP_COPY_FROM_INNER is supported for tunnel decap\n");
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_SUPPORTED;
    }
    *has_decap_attr = true;

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_ECN_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_ECN_MODE_KEEP_INNER is supported for tunnel encap\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        *has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_ECN_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_DECAP_ECN_MODE_STANDARD != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_ECN_MODE_KEEP_INNER is supported for tunnel decap\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        *has_decap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_MAPPERS, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        SX_LOG_ERR("encap ecn mappers are not supported on create\n");
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_SUPPORTED;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_MAPPERS, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        SX_LOG_ERR("decap ecn mappers are not supported on create\n");
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_SUPPORTED;
    }

    assert((SAI_TUNNEL_IPINIP == sai_tunnel_type) ||
           (SAI_TUNNEL_IPINIP_GRE == sai_tunnel_type));

    if (SAI_TUNNEL_IPINIP == sai_tunnel_type) {
        sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, &attr, &attr_idx);
        if (SAI_STATUS_SUCCESS == sai_status) {
            SX_LOG_ERR("encap gre key valid are only supported for ip in ip gre on create\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }

        sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY, &attr, &attr_idx);
        if (SAI_STATUS_SUCCESS == sai_status) {
            SX_LOG_ERR("encap gre key are only supported for ip in ip gre on create\n");
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
    } else if (SAI_TUNNEL_IPINIP_GRE == sai_tunnel_type) {
        sdk_ipinip_p2p_attrib->decap.gre_check_key = false;

        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, &attr, &attr_idx))) {
            if (attr->booldata) {
                sdk_ipinip_p2p_attrib->encap.gre_mode = SX_TUNNEL_IPINIP_GRE_MODE_ENABLED_WITH_KEY;

                if (SAI_STATUS_SUCCESS ==
                    (sai_status =
                         find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY, &attr,
                                             &attr_idx))) {
                    sdk_ipinip_p2p_attrib->encap.gre_key = attr->u32;
                } else {
                    SX_LOG_ERR("gre key is missing when encap gre key valid is set to true\n");
                    SX_LOG_EXIT();
                    return SAI_STATUS_FAILURE;
                }
            } else {
                sdk_ipinip_p2p_attrib->encap.gre_mode = SX_TUNNEL_IPINIP_GRE_MODE_ENABLED;
                sdk_ipinip_p2p_attrib->encap.gre_key  = 0;
            }
            *has_encap_attr = true;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_fill_sx_ipinip_p2p_tunnel_data(_In_ sai_tunnel_type_t       sai_tunnel_type,
                                                            _Out_ sx_tunnel_attribute_t *sx_tunnel_attribute,
                                                            _In_ uint32_t                attr_count,
                                                            _In_ const sai_attribute_t  *attr_list)
{
    sai_status_t                      sai_status;
    sx_tunnel_type_e                  sx_type;
    bool                              has_encap_attr = false;
    bool                              has_decap_attr = false;
    sx_tunnel_ipinip_p2p_attribute_t *sdk_ipinip_p2p_attrib;

    SX_LOG_ENTER();

    if (!sx_tunnel_attribute) {
        SX_LOG_ERR("NULL sx_tunnel_attribute\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (!attr_list) {
        SX_LOG_ERR("NULL attr_list\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    memset(sx_tunnel_attribute, 0, sizeof(sx_tunnel_attribute_t));

    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_convert_sai_tunnel_type_to_sx(sai_tunnel_type, &sx_type))) {
        SX_LOG_ERR("Error converting sai tunnel type to sdk tunnel type\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if ((SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4 != sx_type) &&
        (SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE != sx_type)) {
        SX_LOG_ERR("Create sai tunnel using none ip in ip sx type: %d\n", sx_type);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sx_tunnel_attribute->type = sx_type;

    if (SAI_TUNNEL_IPINIP == sai_tunnel_type) {
        sdk_ipinip_p2p_attrib = &sx_tunnel_attribute->attributes.ipinip_p2p;
    } else if (SAI_TUNNEL_IPINIP_GRE == sai_tunnel_type) {
        sdk_ipinip_p2p_attrib = &sx_tunnel_attribute->attributes.ipinip_p2p_gre;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_sdk_fill_ipinip_p2p_attrib(attr_count,
                                                      attr_list,
                                                      sai_tunnel_type,
                                                      sdk_ipinip_p2p_attrib,
                                                      &has_encap_attr,
                                                      &has_decap_attr))) {
        SX_LOG_ERR("Error filling sdk ipinip p2p attribute\n");
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    if (has_encap_attr && !has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_ENCAP;
    } else if (!has_encap_attr && has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_DECAP;
    } else if (has_encap_attr && has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_SYMMETRIC;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_create_vxlan_tunnel_map_list(_In_ sai_object_id_t      *sai_tunnel_mapper_list,
                                                          _In_ uint32_t              sai_tunnel_mapper_cnt,
                                                          _In_ sai_tunnel_map_type_t sai_tunnel_map_type,
                                                          _In_ sai_object_id_t       sai_tunnel_obj_id)
{
    sai_object_id_t       sai_mapper_obj_id = SAI_NULL_OBJECT_ID;
    mlnx_tunnel_map_t     mlnx_tunnel_map;
    sai_status_t          sai_status             = SAI_STATUS_FAILURE;
    sx_status_t           sdk_status             = SX_STATUS_ERROR;
    sx_bridge_id_t        sx_bridge_id           = 0;
    sx_port_log_id_t      sx_log_vport           = 0;
    uint32_t              sai_tunnel_mapper_idx  = 0;
    sai_object_id_t       overlay_bridge_port_id = 0;
    sx_port_log_id_t      sx_bridge_port_id      = 0;
    uint32_t              tunnel_db_idx          = 0;
    uint32_t              ii                     = 0, jj = 0;
    sx_tunnel_map_entry_t sx_tunnel_map_entry;
    const uint32_t        sx_tunnel_map_entry_cnt = 1;
    sx_tunnel_id_t        sx_tunnel_id            = 0;

    SX_LOG_ENTER();

    memset(&sx_tunnel_map_entry, 0, sizeof(sx_tunnel_map_entry_t));

    if (0 == sai_tunnel_mapper_cnt) {
        SX_LOG_ERR("tunnel mapper cnt is zero\n");
        SX_LOG_EXIT();
        return SAI_STATUS_SUCCESS;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_tunnel_db_idx(sai_tunnel_obj_id, &tunnel_db_idx))) {
        SX_LOG_ERR("Error getting sai tunnel db idx from sai tunnel id %" PRIx64 "\n", sai_tunnel_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_db_read_lock();

    sx_bridge_id = g_sai_db_ptr->sx_bridge_id;

    overlay_bridge_port_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_vxlan_overlay_rif;

    sx_tunnel_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sx_tunnel_id;

    sai_db_unlock();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(overlay_bridge_port_id, SAI_OBJECT_TYPE_PORT, &sx_bridge_port_id, NULL))) {
        SX_LOG_ERR("Fail to get bridge port for overlay interface\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    for (ii = 0; ii < sai_tunnel_mapper_cnt; ii++) {
        sai_mapper_obj_id = sai_tunnel_mapper_list[ii];

        sai_db_write_lock();

        if (SAI_STATUS_SUCCESS !=
            (sai_status =
                 mlnx_get_sai_tunnel_map_db_idx(sai_mapper_obj_id, &sai_tunnel_mapper_idx))) {
            sai_db_unlock();
            SX_LOG_ERR("Error getting tunnel mapper db idx from tunnel mapper obj id %" PRIx64 "\n",
                       sai_mapper_obj_id);
            SX_LOG_EXIT();
            return sai_status;
        }

        g_sai_db_ptr->mlnx_tunnel_map[sai_tunnel_mapper_idx].tunnel_cnt++;

        sai_db_unlock();

        SX_LOG_DBG("ii: %d, cnt: %d, sai_mapper_obj_id: %" PRIx64 "\n", ii, sai_tunnel_mapper_cnt, sai_mapper_obj_id);

        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_tunnel_map_db_param_get_from_db(sai_mapper_obj_id, &mlnx_tunnel_map))) {
            SX_LOG_ERR("Fail to get mlnx tunnel map for tunnel map obj id %" PRIx64 "\n", sai_mapper_obj_id);
            SX_LOG_EXIT();
            return sai_status;
        }

        SX_LOG_NTC("Tunnel map list count: %d\n", mlnx_tunnel_map.tunnel_map_list_count);

        for (jj = 0; jj < mlnx_tunnel_map.tunnel_map_list_count; jj++) {
            if (sai_tunnel_map_type != mlnx_tunnel_map.tunnel_map_type) {
                SX_LOG_ERR("sai tunnel map type should be %d but getting %d\n",
                           sai_tunnel_map_type,
                           mlnx_tunnel_map.tunnel_map_type);
                SX_LOG_EXIT();
                return SAI_STATUS_FAILURE;
            }

            SX_LOG_DBG("jj: %d\n", jj);

            /* TODO: change the logic here after SAI bridge port being officially introduced */
            if (SX_STATUS_SUCCESS !=
                (sai_status =
                     sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_bridge_port_id,
                                           mlnx_tunnel_map.tunnel_map_list[jj].key.vlan_id, &sx_log_vport))) {
                SX_LOG_ERR("Error setting vport of port, SX STATUS: %s\n", SX_STATUS_MSG(sai_status));
                SX_LOG_EXIT();
                return sai_status;
            }

            if (SX_STATUS_SUCCESS !=
                (sai_status = sx_api_port_state_set(gh_sdk, sx_log_vport, SX_PORT_ADMIN_STATUS_UP))) {
                SX_LOG_ERR("Error setting vport admin state, SX STATUS: %s\n", SX_STATUS_MSG(sai_status));
                SX_LOG_EXIT();
                return sai_status;
            }

            if (SX_STATUS_SUCCESS !=
                (sai_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_bridge_id, sx_log_vport))) {
                SX_LOG_ERR("Error setting sx bridge vport, SX STATUS: %s\n", SX_STATUS_MSG(sai_status));
                SX_LOG_EXIT();
                return sai_status;
            }

            SX_LOG_NTC("Set bridge port for bridge id %x\n", sx_bridge_id);

            sx_tunnel_map_entry.type                 = SX_TUNNEL_TYPE_NVE_VXLAN;
            sx_tunnel_map_entry.params.nve.bridge_id = sx_bridge_id;
            sx_tunnel_map_entry.params.nve.vni       = mlnx_tunnel_map.tunnel_map_list[jj].value.vni_id;
            sx_tunnel_map_entry.params.nve.direction = SX_TUNNEL_MAP_DIR_BIDIR;

            if (SX_STATUS_SUCCESS !=
                (sdk_status = sx_api_tunnel_map_set(gh_sdk,
                                                    SX_ACCESS_CMD_ADD,
                                                    sx_tunnel_id,
                                                    &sx_tunnel_map_entry,
                                                    sx_tunnel_map_entry_cnt))) {
                sai_status = sdk_to_sai(sdk_status);
                SX_LOG_ERR("Error adding tunnel map associated with sx tunnel id %d, sx status %s\n",
                           sx_tunnel_id, SX_STATUS_MSG(sdk_status));
                return sai_status;
            }
        }
    }

    SX_LOG_EXIT();

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_sai_fill_sx_vxlan_tunnel_data(_In_ sai_tunnel_type_t       sai_tunnel_type,
                                                       _In_ uint32_t                attr_count,
                                                       _In_ const sai_attribute_t  *attr_list,
                                                       _Out_ sx_tunnel_attribute_t *sx_tunnel_attribute,
                                                       _Out_ tunnel_db_entry_t     *mlnx_tunnel_db_entry)
{
    sai_status_t                 sai_status;
    sx_tunnel_type_e             sx_type;
    uint32_t                     data;
    const sai_attribute_value_t *attr;
    uint32_t                     attr_idx;
    bool                         has_encap_attr = false;
    bool                         has_decap_attr = false;
    sx_router_id_t               sdk_vrid;

    SX_LOG_ENTER();

    if (!sx_tunnel_attribute) {
        SX_LOG_ERR("NULL sx_tunnel_attribute\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (!attr_list) {
        SX_LOG_ERR("NULL attr_list\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }
    memset(sx_tunnel_attribute, 0, sizeof(sx_tunnel_attribute_t));
    memset(mlnx_tunnel_db_entry, 0, sizeof(tunnel_db_entry_t));

    if (SAI_STATUS_SUCCESS != (sai_status = mlnx_convert_sai_tunnel_type_to_sx(sai_tunnel_type, &sx_type))) {
        SX_LOG_ERR("Error converting sai tunnel type to sdk tunnel type\n");
        SX_LOG_EXIT();
        return sai_status;
    }
    if (SX_TUNNEL_TYPE_NVE_VXLAN != sx_type) {
        SX_LOG_ERR("Create sai tunnel using none vxlan sx type: %d\n", sx_type);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sx_tunnel_attribute->type = sx_type;

    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_OVERLAY_INTERFACE, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_PORT, &data, NULL))) {
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }
        mlnx_tunnel_db_entry->sai_vxlan_overlay_rif = attr->oid;
    } else {
        SX_LOG_ERR("overlay interface should be specified on creating vxlan type tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status =
             find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_ROUTER_INTERFACE, &data, NULL))) {
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }

        mlnx_tunnel_db_entry->sai_vxlan_underlay_rif = attr->oid;

        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_sai_get_sx_vrid_from_sx_rif((sx_router_interface_t)data, &sdk_vrid))) {
            SX_LOG_ERR("mlnx_sai_get_sx_vrid_from_sx_rif failed\n");
            SX_LOG_EXIT();
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        }

        sx_tunnel_attribute->attributes.vxlan.encap.underlay_vrid = sdk_vrid;
    } else {
        SX_LOG_ERR("underlay interface should be specified on creating vxlan type tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
    }

    if (SAI_STATUS_SUCCESS ==
        (sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_SRC_IP, &attr, &attr_idx))) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_translate_sai_ip_address_to_sdk(&attr->ipaddr,
                                                               &sx_tunnel_attribute->attributes.vxlan.encap.
                                                               underlay_sip))) {
            SX_LOG_ERR("Error setting src ip on creating tunnel table\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_TTL_PIPE_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_TTL_PIPE_MODEL is supported for tunnel encap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_DSCP_UNIFORM_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_DSCP_UNIFORM_MODEL is supported for tunnel encap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_TTL_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_TTL_PIPE_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_TTL_PIPE_MODEL is supported for tunnel decap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_decap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_DSCP_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_DSCP_UNIFORM_MODEL != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_DSCP_UNIFORM_MODEL is supported for tunnel decap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_decap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_ECN_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD is supported for tunnel encap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_ECN_MODE, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        if (SAI_TUNNEL_DECAP_ECN_MODE_STANDARD != attr->s32) {
            SX_LOG_ERR("Only SAI_TUNNEL_DECAP_ECN_MODE_STANDARD is supported for tunnel decap, but getting %d\n",
                       attr->s32);
            SX_LOG_EXIT();
            return SAI_STATUS_NOT_SUPPORTED;
        }
        has_decap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_MAPPERS, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        memcpy(mlnx_tunnel_db_entry->sai_tunnel_map_encap_id_array,
               attr->objlist.list,
               attr->objlist.count * sizeof(sai_object_id_t));
        mlnx_tunnel_db_entry->sai_tunnel_map_encap_cnt = attr->objlist.count;

        SX_LOG_NTC("encap map cnt: %d\n", attr->objlist.count);

        has_encap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_DECAP_MAPPERS, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        memcpy(mlnx_tunnel_db_entry->sai_tunnel_map_decap_id_array,
               attr->objlist.list,
               attr->objlist.count * sizeof(sai_object_id_t));
        mlnx_tunnel_db_entry->sai_tunnel_map_decap_cnt = attr->objlist.count;

        SX_LOG_NTC("decap map cnt: %d\n", attr->objlist.count);

        has_decap_attr = true;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        SX_LOG_ERR("encap gre key valid is not valid for vxlan tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY, &attr, &attr_idx);
    if (SAI_STATUS_SUCCESS == sai_status) {
        SX_LOG_ERR("encap gre key is not valid for vxlan tunnel\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (has_encap_attr && !has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_ENCAP;
    } else if (!has_encap_attr && has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_DECAP;
    } else if (has_encap_attr && has_decap_attr) {
        sx_tunnel_attribute->direction = SX_TUNNEL_DIRECTION_SYMMETRIC;
    }

    sai_db_read_lock();

    sx_tunnel_attribute->attributes.vxlan.nve_log_port = g_sai_db_ptr->sx_nve_log_port;

    sai_db_unlock();

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/* caller needs to guard this function with lock */
static sai_status_t mlnx_fill_vxlan_tunnel_db(_In_ sai_object_id_t    sai_tunnel_obj_id,
                                              _In_ tunnel_db_entry_t *mlnx_tunnel_db_entry)
{
    sai_status_t sai_status    = SAI_STATUS_FAILURE;
    uint32_t     tunnel_db_idx = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_tunnel_db_idx(sai_tunnel_obj_id, &tunnel_db_idx))) {
        SX_LOG_ERR("Error getting sai tunnel db idx from sai tunnel id %" PRIx64 "\n", sai_tunnel_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_vxlan_overlay_rif  = mlnx_tunnel_db_entry->sai_vxlan_overlay_rif;
    g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_vxlan_underlay_rif = mlnx_tunnel_db_entry->sai_vxlan_underlay_rif;
    memcpy(g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_id_array,
           mlnx_tunnel_db_entry->sai_tunnel_map_encap_id_array,
           mlnx_tunnel_db_entry->sai_tunnel_map_encap_cnt * sizeof(sai_object_id_t));
    g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_cnt = mlnx_tunnel_db_entry->sai_tunnel_map_encap_cnt;
    g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_cnt = mlnx_tunnel_db_entry->sai_tunnel_map_decap_cnt;
    memcpy(g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_id_array,
           mlnx_tunnel_db_entry->sai_tunnel_map_decap_id_array,
           mlnx_tunnel_db_entry->sai_tunnel_map_decap_cnt * sizeof(sai_object_id_t));

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_create_tunnel(_Out_ sai_object_id_t     * sai_tunnel_obj_id,
                                       _In_ uint32_t               attr_count,
                                       _In_ const sai_attribute_t* attr_list)
{
    SX_LOG_ENTER();
    sai_status_t                 sai_status;
    const sai_attribute_value_t *attr;
    uint32_t                     attr_idx;
    char                         list_str[MAX_LIST_VALUE_STR_LEN] = { 0 };
    sx_status_t                  sdk_status;
    sx_tunnel_attribute_t        sx_tunnel_attr;
    sx_tunnel_id_t               sx_tunnel_id;
    sai_tunnel_type_t            sai_tunnel_type;
    sx_router_interface_state_t  rif_state;
    bool                         sdk_tunnel_map_created = false;
    bool                         sdk_tunnel_created     = false;
    bool                         sai_db_created         = false;
    tunnel_db_entry_t            mlnx_tunnel_db_entry;
    uint32_t                     tunnel_db_idx = 0;
    tunnel_db_entry_t            sai_tunnel_db_entry;
    sx_tunnel_map_entry_t        sx_tunnel_map_entry[MLNX_TUNNEL_MAP_MAX];

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             check_attribs_metadata(attr_count, attr_list, tunnel_attribs, tunnel_vendor_attribs,
                                    SAI_COMMON_API_CREATE))) {
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = sai_attr_list_to_str(attr_count, attr_list, tunnel_attribs, MAX_LIST_VALUE_STR_LEN, list_str))) {
        SX_LOG_EXIT();
        return sai_status;
    }
    SX_LOG_NTC("Create tunnel attribs, %s\n", list_str);

    memset(&sx_tunnel_attr, 0, sizeof(sx_tunnel_attr));

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_ATTR_TYPE, &attr, &attr_idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    sai_tunnel_type = attr->s32;
    switch (sai_tunnel_type) {
    case SAI_TUNNEL_IPINIP:
    case SAI_TUNNEL_IPINIP_GRE:
        if (SAI_STATUS_SUCCESS != (sai_status =
                                       mlnx_sai_fill_sx_ipinip_p2p_tunnel_data(
                                           sai_tunnel_type,
                                           &sx_tunnel_attr,
                                           attr_count,
                                           attr_list))) {
            SX_LOG_ERR("Failed to fill sx ipinip p2p tunnel data\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        break;

    case SAI_TUNNEL_VXLAN:
        if (SAI_STATUS_SUCCESS != (sai_status =
                                       mlnx_sai_fill_sx_vxlan_tunnel_data(
                                           sai_tunnel_type,
                                           attr_count,
                                           attr_list,
                                           &sx_tunnel_attr,
                                           &mlnx_tunnel_db_entry))) {
            SX_LOG_ERR("Failed to fill sx vxlan tunnel data\n");
            SX_LOG_EXIT();
            return sai_status;
        }
        break;

    case SAI_TUNNEL_MPLS:
        SX_LOG_ERR("Tunnel MPLS type is not supported yet\n");
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_IMPLEMENTED;
        break;

    default:
        SX_LOG_EXIT();
        return SAI_STATUS_NOT_SUPPORTED;
    }

    if (SX_STATUS_SUCCESS != (sdk_status = sx_api_tunnel_set(
                                  gh_sdk,
                                  SX_ACCESS_CMD_CREATE,
                                  &sx_tunnel_attr,
                                  &sx_tunnel_id))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error creating sdk tunnel, sx status: %s\n", SX_STATUS_MSG(sdk_status));
        SX_LOG_EXIT();
        return sai_status;
    }
    sdk_tunnel_created = true;

    sai_db_write_lock();
    sai_status = mlnx_sai_tunnel_create_tunnel_object_id(sx_tunnel_id, sai_tunnel_obj_id);

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error create tunnel object id from sx tunnel id %d\n", sx_tunnel_id);
        sai_db_unlock();
        goto cleanup;
    }
    sai_db_created = true;

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_sai_tunnel_db_idx(*sai_tunnel_obj_id, &tunnel_db_idx))) {
        sai_db_unlock();
        SX_LOG_ERR("Error getting sai tunnel db idx from sai tunnel id %" PRIx64 "\n", *sai_tunnel_obj_id);
        goto cleanup;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_tunnel_db_entry(*sai_tunnel_obj_id, &sai_tunnel_db_entry))) {
        sai_db_unlock();
        SX_LOG_ERR("Error getting sai tunnel db entry for sai tunnel obj id %" PRIx64 "\n", *sai_tunnel_obj_id);
        goto cleanup;
    }

    sai_db_unlock();

    if (SAI_TUNNEL_VXLAN == sai_tunnel_type) {
        sai_db_write_lock();

        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_fill_vxlan_tunnel_db(*sai_tunnel_obj_id, &mlnx_tunnel_db_entry))) {
            sai_db_unlock();
            SX_LOG_ERR("Failed to fill in vxlan tunnel db for sai tunnel obj id %" PRIx64 "\n", *sai_tunnel_obj_id);
            goto cleanup;
        }

        sai_db_unlock();

        sdk_tunnel_map_created = true;

        sai_status = mlnx_sai_create_vxlan_tunnel_map_list(mlnx_tunnel_db_entry.sai_tunnel_map_encap_id_array,
                                                           mlnx_tunnel_db_entry.sai_tunnel_map_encap_cnt,
                                                           SAI_TUNNEL_MAP_VLAN_ID_TO_VNI,
                                                           *sai_tunnel_obj_id);
        if (SAI_STATUS_SUCCESS != sai_status) {
            SX_LOG_ERR("Failed to create sai vxlan encap tunnel map list\n");
            goto cleanup;
        }

        sai_status = mlnx_sai_create_vxlan_tunnel_map_list(mlnx_tunnel_db_entry.sai_tunnel_map_decap_id_array,
                                                           mlnx_tunnel_db_entry.sai_tunnel_map_decap_cnt,
                                                           SAI_TUNNEL_MAP_VNI_TO_VLAN_ID,
                                                           *sai_tunnel_obj_id);
        if (SAI_STATUS_SUCCESS != sai_status) {
            SX_LOG_ERR("Failed to create sai vxlan decap tunnel map list\n");
            goto cleanup;
        }
    }

    SX_LOG_NTC("created tunnel:0x%" PRIx64 "\n", *sai_tunnel_obj_id);

    memset(&rif_state, 0, sizeof(sx_router_interface_state_t));

    rif_state.ipv4_enable = true;
    rif_state.ipv6_enable = true;

    if ((SAI_TUNNEL_IPINIP == sai_tunnel_type) ||
        (SAI_TUNNEL_IPINIP_GRE == sai_tunnel_type)) {
        if (SX_STATUS_SUCCESS !=
            (sdk_status =
                 sx_api_router_interface_state_set(gh_sdk, sx_tunnel_attr.attributes.ipinip_p2p.overlay_rif,
                                                   &rif_state))) {
            SX_LOG_ERR("Failed to set overlay router interface state - %s.\n", SX_STATUS_MSG(sai_status));
            sai_status = sdk_to_sai(sdk_status);
            goto cleanup;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;

cleanup:
    if (sai_db_created) {
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(*sai_tunnel_obj_id, SAI_OBJECT_TYPE_TUNNEL, &tunnel_db_idx,
                                              NULL))) {
            SX_LOG_ERR("Invalid sai tunnel obj id: 0x%" PRIx64 "\n", *sai_tunnel_obj_id);
        } else {
            if (tunnel_db_idx >= MAX_TUNNEL_DB_SIZE) {
                SX_LOG_ERR("tunnel db index: %d out of bounds:%d\n", tunnel_db_idx, MAX_TUNNEL_DB_SIZE);
                sai_status = SAI_STATUS_FAILURE;
            } else {
                memset(&g_sai_db_ptr->tunnel_db[tunnel_db_idx], 0, sizeof(tunnel_db_entry_t));
            }
        }
    }

    if (sdk_tunnel_map_created) {
        if (SX_STATUS_SUCCESS !=
            (sdk_status =
                 sx_api_tunnel_map_set(gh_sdk,
                                       SX_ACCESS_CMD_DELETE_ALL,
                                       sx_tunnel_id,
                                       sx_tunnel_map_entry,
                                       0))) {
            sai_status = sdk_to_sai(sdk_status);
            SX_LOG_ERR("Error deleting all tunnel map associated with sx tunnel id %d, sx status %s\n",
                       sx_tunnel_id, SX_STATUS_MSG(sdk_status));
        }
    }

    if (sdk_tunnel_created) {
        if (SX_STATUS_SUCCESS !=
            (sdk_status = sx_api_tunnel_set(gh_sdk,
                                            SX_ACCESS_CMD_DESTROY,
                                            &sx_tunnel_attr,
                                            &sx_tunnel_id))) {
            sai_status = sdk_to_sai(sdk_status);
            SX_LOG_ERR("Error destroying sx tunnel id %d, sx status: %s\n", sx_tunnel_id, SX_STATUS_MSG(sdk_status));
        }
    }

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_remove_tunnel(_In_ const sai_object_id_t sai_tunnel_obj_id)
{
    sai_status_t                sai_status    = SAI_STATUS_FAILURE;
    sx_status_t                 sdk_status    = SX_STATUS_ERROR;
    uint32_t                    tunnel_db_idx = 0;
    sx_tunnel_id_t              sx_tunnel_id  = 0;
    sx_tunnel_attribute_t       sx_tunnel_attr;
    sx_router_interface_state_t rif_state;
    sx_tunnel_map_entry_t       sx_tunnel_map_entry;
    sai_object_id_t             sai_tunnel_map_id  = 0;
    uint32_t                    ii                 = 0;
    uint32_t                    sai_tunnel_map_idx = 0;
    sx_port_log_id_t            log_port           = 0;
    sx_port_log_id_t            log_vport          = 0;
    sx_vlan_id_t                vlan_id            = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(sai_tunnel_obj_id, SAI_OBJECT_TYPE_TUNNEL, &tunnel_db_idx,
                                 NULL))) {
        SX_LOG_ERR("Invalid sai tunnel obj id: 0x%" PRIx64 "\n", sai_tunnel_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (tunnel_db_idx >= MAX_TUNNEL_DB_SIZE) {
        SX_LOG_ERR("tunnel db index: %d out of bounds:%d\n", tunnel_db_idx, MAX_TUNNEL_DB_SIZE);
        SX_LOG_EXIT();
        return SAI_STATUS_FAILURE;
    }

    sai_db_write_lock();

    if (!g_sai_db_ptr->tunnel_db[tunnel_db_idx].is_used) {
        SX_LOG_ERR("Tunnel db ind %d cannot be removed because it is not used\n", tunnel_db_idx);
        sai_status = SAI_STATUS_FAILURE;
        goto cleanup;
    }

    sx_tunnel_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sx_tunnel_id;

    memset(&sx_tunnel_attr, 0, sizeof(sx_tunnel_attribute_t));

    if (SX_STATUS_SUCCESS != (sdk_status = sx_api_tunnel_get(
                                  gh_sdk,
                                  sx_tunnel_id,
                                  &sx_tunnel_attr))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error getting sx tunnel attr from sx tunnel id %d, sx status: %s\n", sx_tunnel_id,
                   SX_STATUS_MSG(sdk_status));
        goto cleanup;
    }

    memset(&rif_state, 0, sizeof(sx_router_interface_state_t));

    if ((SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE == sx_tunnel_attr.type) ||
        (SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4 == sx_tunnel_attr.type)) {
        rif_state.ipv4_enable = false;
        rif_state.ipv6_enable = false;

        if (SX_STATUS_SUCCESS !=
            (sdk_status =
                 sx_api_router_interface_state_set(gh_sdk, sx_tunnel_attr.attributes.ipinip_p2p.overlay_rif,
                                                   &rif_state))) {
            SX_LOG_ERR("Failed to set overlay router interface state to down - %s.\n", SX_STATUS_MSG(sdk_status));
            sai_status = sdk_to_sai(sdk_status);
            goto cleanup;
        }
    }

    if (SX_TUNNEL_TYPE_NVE_VXLAN == sx_tunnel_attr.type) {
        if ((0 != g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_cnt) ||
            (0 != g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_cnt)) {
            if (SX_STATUS_SUCCESS != (sdk_status = sx_api_tunnel_map_set(
                                          gh_sdk,
                                          SX_ACCESS_CMD_DELETE_ALL,
                                          sx_tunnel_id,
                                          &sx_tunnel_map_entry,
                                          0))) {
                sai_status = sdk_to_sai(sdk_status);
                SX_LOG_ERR("Error deleting all tunnel map associated with sx tunnel id %d, sx status %s\n",
                           sx_tunnel_id, SX_STATUS_MSG(sdk_status));
                goto cleanup;
            }

            for (ii = 0; ii < g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_cnt; ii++) {
                sai_tunnel_map_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_id_array[ii];
                if (SAI_STATUS_SUCCESS !=
                    (sai_status =
                         mlnx_get_sai_tunnel_map_db_idx(sai_tunnel_map_id, &sai_tunnel_map_idx))) {
                    SX_LOG_ERR("Error getting tunnel mapper db idx from tunnel mapper obj id %" PRIx64 "\n",
                               sai_tunnel_map_id);
                    goto cleanup;
                }

                assert(0 < g_sai_db_ptr->mlnx_tunnel_map[sai_tunnel_map_idx].tunnel_cnt);
                g_sai_db_ptr->mlnx_tunnel_map[sai_tunnel_map_idx].tunnel_cnt--;
            }

            for (ii = 0; ii < g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_cnt; ii++) {
                sai_tunnel_map_id = g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_id_array[ii];
                if (SAI_STATUS_SUCCESS !=
                    (sai_status =
                         mlnx_get_sai_tunnel_map_db_idx(sai_tunnel_map_id, &sai_tunnel_map_idx))) {
                    SX_LOG_ERR("Error getting tunnel mapper db idx from tunnel mapper obj id %" PRIx64 "\n",
                               sai_tunnel_map_id);
                    goto cleanup;
                }

                assert(0 < g_sai_db_ptr->mlnx_tunnel_map[sai_tunnel_map_idx].tunnel_cnt);
                g_sai_db_ptr->mlnx_tunnel_map[sai_tunnel_map_idx].tunnel_cnt--;
            }

            memset(g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_id_array, 0,
                   sizeof(MLNX_TUNNEL_MAP_MAX));
            g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_encap_cnt = 0;
            memset(g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_id_array, 0,
                   sizeof(MLNX_TUNNEL_MAP_MAX));
            g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_tunnel_map_decap_cnt = 0;
        }
    }

    if (SX_STATUS_SUCCESS != (sdk_status = sx_api_tunnel_set(
                                  gh_sdk,
                                  SX_ACCESS_CMD_DESTROY,
                                  &sx_tunnel_attr,
                                  &sx_tunnel_id))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error destroying sx tunnel id %d, sx status: %s\n", sx_tunnel_id, SX_STATUS_MSG(sdk_status));
        goto cleanup;
    }

    /* Remove the following bridge logic after SAI bridge being officially introduced */
    if (SX_TUNNEL_TYPE_NVE_VXLAN == sx_tunnel_attr.type) {
        if (SX_STATUS_SUCCESS != (sdk_status = sx_api_bridge_vport_set(
                                      gh_sdk,
                                      SX_ACCESS_CMD_DELETE_ALL,
                                      g_sai_db_ptr->sx_bridge_id,
                                      log_vport))) {
            sai_status = sdk_to_sai(sdk_status);
            SX_LOG_ERR("Error deleting all vport for sx bridge id %d, sx status: %s\n",
                       g_sai_db_ptr->sx_bridge_id, SX_STATUS_MSG(sdk_status));
            goto cleanup;
        }

        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_object_to_type(g_sai_db_ptr->tunnel_db[tunnel_db_idx].sai_vxlan_overlay_rif,
                                              SAI_OBJECT_TYPE_PORT,
                                              &log_port, NULL))) {
        }
        if (SX_STATUS_SUCCESS != (sdk_status = sx_api_port_vport_set(
                                      gh_sdk,
                                      SX_ACCESS_CMD_DELETE_ALL,
                                      log_port,
                                      vlan_id,
                                      &log_vport))) {
            sai_status = sdk_to_sai(sdk_status);
            SX_LOG_ERR("Error deleting all vport for sx bridge id %d, sx status: %s\n",
                       g_sai_db_ptr->sx_bridge_id, SX_STATUS_MSG(sdk_status));
            goto cleanup;
        }
    }

    memset(&g_sai_db_ptr->tunnel_db[tunnel_db_idx], 0, sizeof(tunnel_db_entry_t));

    SX_LOG_NTC("removed tunnel:0x%" PRIx64 "\n", sai_tunnel_obj_id);

    sai_status = SAI_STATUS_SUCCESS;

cleanup:
    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_set_tunnel_attribute(_In_ const sai_object_id_t  sai_tunnel_obj_id,
                                              _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_key_to_str(sai_tunnel_obj_id, key_str);
    sai_status = sai_set_attribute(&key, key_str, tunnel_attribs, tunnel_vendor_attribs, attr);

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_get_tunnel_attribute(_In_ const sai_object_id_t sai_tunnel_obj_id,
                                              _In_ uint32_t              attr_count,
                                              _Inout_ sai_attribute_t   *attr_list)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_key_to_str(sai_tunnel_obj_id, key_str);
    sai_status = sai_get_attributes(&key, key_str, tunnel_attribs, tunnel_vendor_attribs, attr_count, attr_list);

    SX_LOG_EXIT();
    return sai_status;
}

/* caller of this function should use read lock to guard the callsite */
static sai_status_t mlnx_create_empty_tunneltable(_Out_ uint32_t *internal_tunneltable_idx)
{
    uint32_t idx = 0;

    SX_LOG_ENTER();

    assert(NULL != g_sai_db_ptr);

    for (idx = 0; idx < MLNX_TUNNELTABLE_SIZE; idx++) {
        if (!g_sai_db_ptr->mlnx_tunneltable[idx].in_use) {
            *internal_tunneltable_idx = idx;
            SX_LOG_EXIT();
            return SAI_STATUS_SUCCESS;
        }
    }

    SX_LOG_ERR(
        "Not enough resources for sai tunnel table entry, at most %d sai tunnel table entrys can be created\n",
        MLNX_TUNNELTABLE_SIZE);

    SX_LOG_EXIT();
    return SAI_STATUS_INSUFFICIENT_RESOURCES;
}

static sai_status_t mlnx_get_tunnel_term_table_entry_attribute_on_create(
    _In_ uint32_t                       attr_count,
    _In_ const sai_attribute_t         *attr_list,
    _Out_ const sai_attribute_value_t **tunneltable_vr_id,
    _Out_ const sai_attribute_value_t **tunneltable_type,
    _Out_ const sai_attribute_value_t **tunneltable_dst_ip,
    _Out_ const sai_attribute_value_t **tunneltable_src_ip,
    _Out_ const sai_attribute_value_t **tunneltable_tunnel_type,
    _Out_ const sai_attribute_value_t **tunneltable_tunnel_id)
{
    char         list_str[MAX_LIST_VALUE_STR_LEN];
    uint32_t     idx        = 0;
    sai_status_t sai_status = SAI_STATUS_FAILURE;

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             check_attribs_metadata(attr_count, attr_list, tunnel_term_table_entry_attribs,
                                    tunnel_term_table_entry_vendor_attribs,
                                    SAI_COMMON_API_CREATE))) {
        SX_LOG_ERR("Tunnel table: metadata check failed\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_attr_list_to_str(attr_count, attr_list, tunnel_term_table_entry_attribs, MAX_LIST_VALUE_STR_LEN, list_str);
    SX_LOG_NTC("Create tunnel table attributes: %s\n", list_str);

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID, tunneltable_vr_id,
                                     &idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    sai_status = find_attrib_in_list(attr_count, attr_list, SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, tunneltable_type,
                                     &idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    sai_status = find_attrib_in_list(attr_count,
                                     attr_list,
                                     SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP,
                                     tunneltable_dst_ip,
                                     &idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    sai_status = find_attrib_in_list(attr_count,
                                     attr_list,
                                     SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP,
                                     tunneltable_src_ip,
                                     &idx);
    if ((SAI_STATUS_SUCCESS != sai_status) && (SAI_TUNNEL_TERM_TABLE_ENTRY_P2P == (*tunneltable_type)->s32)) {
        SX_LOG_ERR("Tunnel table src ip is missing on creating P2P tunnel table entry\n");
        SX_LOG_EXIT();
        return sai_status;
    } else if ((SAI_STATUS_SUCCESS == sai_status) && (SAI_TUNNEL_TERM_TABLE_ENTRY_P2P != (*tunneltable_type)->s32)) {
        SX_LOG_ERR("Tunnel table src ip should not exist on creating non-P2P tunnel table entry\n");
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status = find_attrib_in_list(attr_count,
                                     attr_list,
                                     SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE,
                                     tunneltable_tunnel_type,
                                     &idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    sai_status = find_attrib_in_list(attr_count,
                                     attr_list,
                                     SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID,
                                     tunneltable_tunnel_id,
                                     &idx);
    assert(SAI_STATUS_SUCCESS == sai_status);

    return SAI_STATUS_SUCCESS;
}

/* caller needs to make sure all the passed in attribute value pointer are not null */
static sai_status_t mlnx_set_tunnel_table_param(_In_ const sai_attribute_value_t   *tunneltable_vr_id,
                                                _In_ const sai_attribute_value_t   *tunneltable_type,
                                                _In_ const sai_attribute_value_t   *tunneltable_dst_ip,
                                                _In_ const sai_attribute_value_t   *tunneltable_src_ip,
                                                _In_ const sai_attribute_value_t   *tunneltable_tunnel_type,
                                                _In_ const sai_attribute_value_t   *tunneltable_tunnel_id,
                                                _Out_ sx_tunnel_decap_entry_key_t  *sdk_tunnel_decap_key,
                                                _Out_ sx_tunnel_decap_entry_data_t *sdk_tunnel_decap_data)
{
    sai_status_t sai_status = SAI_STATUS_FAILURE;
    uint32_t     sdk_vr_id  = 0;

    SX_LOG_ENTER();

    assert(NULL != tunneltable_vr_id);

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_object_to_type(tunneltable_vr_id->oid, SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &sdk_vr_id, NULL))) {
        SX_LOG_ERR("Invalid sai virtual router id %" PRIx64 "\n", tunneltable_vr_id->oid);
        SX_LOG_EXIT();
        return sai_status;
    }

    sdk_tunnel_decap_key->underlay_vrid = (sx_router_id_t)sdk_vr_id;

    assert(NULL != tunneltable_type);

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_convert_sai_tunneltable_type_to_sx(tunneltable_type->s32,
                                                              &sdk_tunnel_decap_key->type))) {
        SX_LOG_ERR("Error converting sai tunnel table entry type %d to sdk tunnel table type\n",
                   tunneltable_type->s32);
        SX_LOG_EXIT();
        return sai_status;
    }

    assert(NULL != tunneltable_dst_ip);
    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_translate_sai_ip_address_to_sdk(&tunneltable_dst_ip->ipaddr,
                                                           &sdk_tunnel_decap_key->underlay_dip))) {
        SX_LOG_ERR("Error setting dst ip on creating tunnel table\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_TUNNEL_TERM_TABLE_ENTRY_P2P == tunneltable_type->s32) {
        assert(NULL != tunneltable_src_ip);
        if (SAI_STATUS_SUCCESS !=
            (sai_status = mlnx_translate_sai_ip_address_to_sdk(&tunneltable_src_ip->ipaddr,
                                                               &sdk_tunnel_decap_key->underlay_sip))) {
            SX_LOG_ERR("Error setting src ip on creating tunnel table\n");
            SX_LOG_EXIT();
            return sai_status;
        }
    }

    assert(NULL != tunneltable_tunnel_type);

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_convert_sai_tunnel_type_to_sx(tunneltable_tunnel_type->s32, &sdk_tunnel_decap_key->tunnel_type))) {
        SX_LOG_ERR("Error converting sai tunnel type %d to sdk tunnel type\n", tunneltable_tunnel_type->s32);
        SX_LOG_EXIT();
        return sai_status;
    }

    assert(NULL != tunneltable_tunnel_id);

    sai_db_read_lock();

    sai_status = mlnx_sai_tunnel_to_sx_tunnel_id(tunneltable_tunnel_id->oid, &sdk_tunnel_decap_data->tunnel_id);

    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Error coverting sai tunnel id %" PRIx64 " to sx tunnel id\n", tunneltable_tunnel_id->oid);
        SX_LOG_EXIT();
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sdk_tunnel_decap_data->action     = SX_ROUTER_ACTION_FORWARD;
    sdk_tunnel_decap_data->counter_id = SX_FLOW_COUNTER_ID_INVALID;
    /* sdk_tunnel_decap_data->trap_attr is ignored when action is set to forward */

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_create_tunnel_term_table_entry(_Out_ sai_object_id_t      *sai_tunnel_term_table_entry_obj_id,
                                                        _In_ uint32_t               attr_count,
                                                        _In_ const sai_attribute_t *attr_list)
{
    const sai_attribute_value_t *tunneltable_vr_id        = NULL, *tunneltable_type = NULL, *tunneltable_dst_ip = NULL,
    *tunneltable_src_ip                                   = NULL;
    const sai_attribute_value_t *tunneltable_tunnel_type  = NULL, *tunneltable_tunnel_id = NULL;
    uint32_t                     internal_tunneltable_idx = 0;
    sx_tunnel_decap_entry_key_t  sdk_tunnel_decap_key;
    sx_tunnel_decap_entry_data_t sdk_tunnel_decap_data;

    memset(&sdk_tunnel_decap_key, 0, sizeof(sx_tunnel_decap_entry_key_t));
    memset(&sdk_tunnel_decap_data, 0, sizeof(sx_tunnel_decap_entry_data_t));
    sai_status_t sai_status  = SAI_STATUS_FAILURE;
    sx_status_t  sdk_status  = SX_STATUS_ERROR;
    bool         cleanup_sdk = false;
    bool         cleanup_db  = false;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_get_tunnel_term_table_entry_attribute_on_create(attr_count, attr_list,
                                                                           &tunneltable_vr_id, &tunneltable_type,
                                                                           &tunneltable_dst_ip, &tunneltable_src_ip,
                                                                           &tunneltable_tunnel_type,
                                                                           &tunneltable_tunnel_id))) {
        SX_LOG_ERR("Failed to get sai tunnel term table entry attribute on create\n");
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_set_tunnel_table_param(tunneltable_vr_id, tunneltable_type, tunneltable_dst_ip, tunneltable_src_ip,
                                         tunneltable_tunnel_type, tunneltable_tunnel_id,
                                         &sdk_tunnel_decap_key, &sdk_tunnel_decap_data))) {
        SX_LOG_ERR("Failed to set tunnel table param for internal tunnel table idx %d\n", internal_tunneltable_idx);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_STATUS_SUCCESS !=
        (sdk_status = sx_api_tunnel_decap_rules_set(gh_sdk, SX_ACCESS_CMD_CREATE,
                                                    &sdk_tunnel_decap_key,
                                                    &sdk_tunnel_decap_data))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error setting tunnel table entry on create, sx status: %s\n", SX_STATUS_MSG(sdk_status));
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_db_write_lock();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_create_empty_tunneltable(&internal_tunneltable_idx))) {
        SX_LOG_ERR("Failed to create empty tunnel table entry\n");
        cleanup_sdk = true;
        goto cleanup;
    }

    SX_LOG_DBG("Created internal tunnel table entry idx: %d\n", internal_tunneltable_idx);

    memset(&g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_idx], 0, sizeof(mlnx_tunneltable_t));

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY, internal_tunneltable_idx, NULL,
                                sai_tunnel_term_table_entry_obj_id))) {
        SX_LOG_ERR("Error creating sai tunnel table entry id from internal tunnel table entry id %d\n",
                   internal_tunneltable_idx);
        cleanup_db  = true;
        cleanup_sdk = true;
        goto cleanup;
    }

    g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_idx].in_use = true;
    memcpy(&g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_idx].sdk_tunnel_decap_key,
           &sdk_tunnel_decap_key,
           sizeof(sx_tunnel_decap_entry_key_t));

    SX_LOG_NTC("Created SAI tunnel table entry obj id: %" PRIx64 "\n", *sai_tunnel_term_table_entry_obj_id);

    sai_status = SAI_STATUS_SUCCESS;

cleanup:
    if (cleanup_db) {
        memset(&g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_idx], 0,
               sizeof(mlnx_tunneltable_t));
    }

    if (cleanup_sdk) {
        if (SX_STATUS_SUCCESS !=
            (sdk_status = sx_api_tunnel_decap_rules_set(gh_sdk, SX_ACCESS_CMD_DESTROY,
                                                        &sdk_tunnel_decap_key,
                                                        &sdk_tunnel_decap_data))) {
            sai_status = sdk_to_sai(sdk_status);
            SX_LOG_ERR("Error setting tunnel table entry on create, sx status: %s\n", SX_STATUS_MSG(sdk_status));
        }
    }

    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_remove_tunnel_term_table_entry(_In_ const sai_object_id_t sai_tunnel_term_table_entry_obj_id)
{
    sai_status_t                 sai_status               = SAI_STATUS_FAILURE;
    sx_status_t                  sdk_status               = SX_STATUS_ERROR;
    uint32_t                     internal_tunneltable_idx = 0;
    sx_tunnel_decap_entry_key_t  sdk_tunnel_decap_key;
    sx_tunnel_decap_entry_data_t sdk_tunnel_decap_data;

    memset(&sdk_tunnel_decap_data, 0, sizeof(sx_tunnel_decap_entry_data_t));

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status =
             mlnx_object_to_type(sai_tunnel_term_table_entry_obj_id, SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
                                 &internal_tunneltable_idx,
                                 NULL))) {
        SX_LOG_ERR("Invalid sai tunnel table entry obj id: %" PRIx64 "\n", sai_tunnel_term_table_entry_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_db_read_lock();

    sai_status = mlnx_tunnel_term_table_entry_sdk_param_get(sai_tunnel_term_table_entry_obj_id, &sdk_tunnel_decap_key);

    sai_db_unlock();

    if (SAI_STATUS_SUCCESS != sai_status) {
        SX_LOG_ERR("Fail to get sdk param for tunnel term table entry id %" PRIx64 "\n",
                   sai_tunnel_term_table_entry_obj_id);
        SX_LOG_EXIT();
        return sai_status;
    }

    if (SX_STATUS_SUCCESS !=
        (sdk_status = sx_api_tunnel_decap_rules_set(gh_sdk, SX_ACCESS_CMD_DESTROY,
                                                    &sdk_tunnel_decap_key,
                                                    &sdk_tunnel_decap_data))) {
        sai_status = sdk_to_sai(sdk_status);
        SX_LOG_ERR("Error setting tunnel table entry on removal, sx status: %s\n", SX_STATUS_MSG(sdk_status));
        SX_LOG_EXIT();
        return sai_status;
    }

    sai_db_write_lock();

    memset(&g_sai_db_ptr->mlnx_tunneltable[internal_tunneltable_idx], 0,
           sizeof(mlnx_tunneltable_t));

    sai_db_unlock();

    SX_LOG_NTC("Removed SAI tunnel table entry obj id %" PRIx64 "\n", sai_tunnel_term_table_entry_obj_id);

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_set_tunnel_map_attribute(_In_ const sai_object_id_t  sai_tunnel_map_obj_id,
                                                  _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_map_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_map_key_to_str(sai_tunnel_map_obj_id, key_str);

    sai_status = sai_set_attribute(&key, key_str, tunnel_map_attribs, tunnel_map_vendor_attribs, attr);

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_get_tunnel_map_attribute(_In_ const sai_object_id_t sai_tunnel_map_obj_id,
                                                  _In_ uint32_t              attr_count,
                                                  _Inout_ sai_attribute_t   *attr_list)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_map_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_map_key_to_str(sai_tunnel_map_obj_id, key_str);

    sai_status =
        sai_get_attributes(&key, key_str, tunnel_map_attribs, tunnel_map_vendor_attribs, attr_count, attr_list);

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_set_tunnel_term_table_entry_attribute(
    _In_ const sai_object_id_t  sai_tunnel_term_table_entry_obj_id,
    _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_term_table_entry_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_term_table_entry_key_to_str(sai_tunnel_term_table_entry_obj_id, key_str);

    sai_status = sai_set_attribute(&key,
                                   key_str,
                                   tunnel_term_table_entry_attribs,
                                   tunnel_term_table_entry_vendor_attribs,
                                   attr);

    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_get_tunnel_term_table_entry_attribute(
    _In_ const sai_object_id_t sai_tunnel_term_table_entry_obj_id,
    _In_ uint32_t              attr_count,
    _Inout_ sai_attribute_t   *attr_list)
{
    const sai_object_key_t key = { .object_id = sai_tunnel_term_table_entry_obj_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           sai_status = SAI_STATUS_FAILURE;

    SX_LOG_ENTER();

    tunnel_term_table_entry_key_to_str(sai_tunnel_term_table_entry_obj_id, key_str);

    sai_status = sai_get_attributes(&key,
                                    key_str,
                                    tunnel_term_table_entry_attribs,
                                    tunnel_term_table_entry_vendor_attribs,
                                    attr_count,
                                    attr_list);

    SX_LOG_EXIT();
    return sai_status;
}

sai_status_t mlnx_tunnel_log_set(sx_verbosity_level_t level)
{
    LOG_VAR_NAME(__MODULE__) = level;

    if (gh_sdk) {
        return sdk_to_sai(sx_api_tunnel_log_verbosity_level_set(gh_sdk, SX_LOG_VERBOSITY_BOTH, level, level));
    } else {
        return SAI_STATUS_SUCCESS;
    }
}

const sai_tunnel_api_t mlnx_tunnel_api = {
    mlnx_create_tunnel_map,
    mlnx_remove_tunnel_map,
    mlnx_set_tunnel_map_attribute,
    mlnx_get_tunnel_map_attribute,
    mlnx_create_tunnel,
    mlnx_remove_tunnel,
    mlnx_set_tunnel_attribute,
    mlnx_get_tunnel_attribute,
    mlnx_create_tunnel_term_table_entry,
    mlnx_remove_tunnel_term_table_entry,
    mlnx_set_tunnel_term_table_entry_attribute,
    mlnx_get_tunnel_term_table_entry_attribute
};
