/******************************************************************************
  @file    qcril_qmi_lte_direct_disc_packing.c
  @brief   qcril qmi - lte direct discovery packing

  DESCRIPTION
    Handles lte direct discovery message packing/unpacking related functions.

  ---------------------------------------------------------------------------
  * Copyright (c) 2016 Qualcomm Technologies, Inc.
  * All Rights Reserved.
  * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ---------------------------------------------------------------------------
******************************************************************************/

/*===========================================================================
                           INCLUDE FILES
===========================================================================*/

#include <pb_decode.h>
#include <pb_encode.h>
#include "qcril_qmi_lte_direct_disc_packing.h"
#include "qcril_log.h"
#include "qcril_qmi_lte_direct_disc_msg_meta.h"
#include "qcril_qmi_npb_encode.h"
#include "qcril_qmi_npb_decode.h"

/*===========================================================================
                                FUNCTIONS
===========================================================================*/

//===========================================================================
// qcril_qmi_lte_direct_disc_pack_msg_tag
//===========================================================================
size_t qcril_qmi_lte_direct_disc_pack_msg_tag(
    const void *msg,
    int msg_len,
    uint32_t token,
    LteDirectDiscovery_MsgType type,
    LteDirectDiscovery_MsgId message_id,
    LteDirectDiscovery_Error error,
    uint8_t *buf, size_t buf_size)
{
  QCRIL_LOG_FUNC_ENTRY();
  QCRIL_NOTUSED(msg_len);

  if (NULL == buf)
  {
    QCRIL_LOG_ERROR("buf is NULL");
    return 0;
  }

  size_t packed_size = 0;
  if(qcril_qmi_lte_direct_disc_is_msg_valid(message_id, type))
  {
    QCRIL_LOG_ERROR("encoding the MsgHeader");
    LteDirectDiscovery_MsgHeader msg_tag;
    memset(&msg_tag, 0x00, sizeof(msg_tag));

    msg_tag.token = token;
    msg_tag.type = type;
    msg_tag.id = message_id;
    msg_tag.error = error;
    pb_ostream_t ostream;
    uint8_t *msg_pack_buf = NULL;
    const pb_field_t* fields = qcril_qmi_lte_direct_disc_get_msg_pb_fields(message_id, type);
    qcril_binary_data_type bin_data;
    if (msg == NULL)
    {
      QCRIL_LOG_INFO("Message payload is NULL, assign NULL to payload");
      msg_tag.payload.arg = NULL;
    }
    else if (fields != NULL)
    {
      QCRIL_LOG_INFO("Message payload exists, encode the payload");
      msg_pack_buf = qcril_malloc(buf_size);
      if (NULL == msg_pack_buf)
      {
        QCRIL_LOG_ERROR("malloc the buffer failed");
        return 0;
      }
      ostream = pb_ostream_from_buffer(msg_pack_buf, buf_size);
      if (qcril_qmi_encode_npb(&ostream, fields, msg))
      {
        bin_data.data = msg_pack_buf;
        bin_data.len = ostream.bytes_written;
        QCRIL_LOG_INFO("bin_data_len: %d", (int)bin_data.len);
        msg_tag.payload.arg = (void*)&bin_data;
      }
    }

    QCRIL_LOG_INFO("Filled in the tag, encoding the tag");
    ostream = pb_ostream_from_buffer(buf, buf_size);
    if (qcril_qmi_encode_npb(&ostream, LteDirectDiscovery_MsgHeader_fields, &msg_tag))
    {
      packed_size = ostream.bytes_written;
      QCRIL_LOG_INFO("encoded the tag");
    }
    else
    {
      QCRIL_LOG_ERROR("pb_encode failed");
    }
  }
  else
  {
    QCRIL_LOG_ERROR("Message id and type doesnt co-exist. Cannot encode the tag");
  }
  QCRIL_LOG_FUNC_RETURN_WITH_RET((int) packed_size);
  return packed_size;
} // qcril_qmi_lte_direct_disc_pack_msg_tag

//===========================================================================
// qcril_qmi_lte_direct_disc_unpack_msg_tag
//===========================================================================
LteDirectDiscovery_MsgHeader* qcril_qmi_lte_direct_disc_unpack_msg_tag(
    const uint8_t *buf,
    size_t buf_size)
{
  QCRIL_LOG_FUNC_ENTRY();

  LteDirectDiscovery_MsgHeader* msg_tag_ptr = NULL;

  if (NULL == buf)
  {
    QCRIL_LOG_ERROR("buf is NULL");
  }
  else
  {
    msg_tag_ptr = qcril_malloc(sizeof(*msg_tag_ptr));
    if (msg_tag_ptr)
    {
      pb_istream_t istream;
      istream = pb_istream_from_buffer((uint8_t *)buf, buf_size);
      if(!qcril_qmi_decode_npb(&istream, LteDirectDiscovery_MsgHeader_fields, msg_tag_ptr))
      {
        QCRIL_LOG_ERROR("pb_decode failed");
        qcril_free(msg_tag_ptr);
        return NULL;
      }
    }
    else
    {
      QCRIL_LOG_ERROR("malloc failed");
    }
  }

  QCRIL_LOG_FUNC_RETURN();
  return msg_tag_ptr;
} // qcril_qmi_lte_direct_disc_unpack_msg_tag

//===========================================================================
// qcril_qmi_lte_direct_disc_parse_packed_msg
//===========================================================================
void qcril_qmi_lte_direct_disc_parse_packed_msg(
    LteDirectDiscovery_MsgType msg_type,
    LteDirectDiscovery_MsgId msg_id,
    const pb_callback_t packed_msg,
    size_t msg_size,
    void** unpacked_msg, size_t*unpacked_msg_size_ptr,
    qcril_evt_e_type* event_ptr)
{
  QCRIL_LOG_FUNC_ENTRY();
  QCRIL_NOTUSED(msg_size);

  *unpacked_msg = NULL;
  *unpacked_msg_size_ptr = 0;
  *event_ptr = qcril_qmi_lte_direct_disc_get_msg_event(msg_id, msg_type);

  if (NULL == packed_msg.arg)
  {
    QCRIL_LOG_ERROR("packed_msg is NULL");
  }
  else
  {
    const pb_field_t* fields = qcril_qmi_lte_direct_disc_get_msg_pb_fields(msg_id, msg_type);
    if (fields && qcril_qmi_lte_direct_disc_get_msg_size(msg_id, msg_type))
    {
      *unpacked_msg = qcril_malloc(qcril_qmi_lte_direct_disc_get_msg_size(msg_id, msg_type));
      if (*unpacked_msg)
      {
        pb_istream_t istream;
        qcril_binary_data_type *bin_data_ptr = (qcril_binary_data_type*) packed_msg.arg;
        istream = pb_istream_from_buffer(bin_data_ptr->data, bin_data_ptr->len);
        if(qcril_qmi_decode_npb(&istream, fields, *unpacked_msg))
        {
          *unpacked_msg_size_ptr = qcril_qmi_lte_direct_disc_get_msg_size(msg_id, msg_type);
        }
      }
    }
    else
    {
      QCRIL_LOG_INFO("no payload for mesage %s(%d)",
          qcril_qmi_lte_direct_disc_get_msg_log_str(msg_id, msg_type), msg_id);
    }
  }
  QCRIL_LOG_FUNC_RETURN();
} // qcril_qmi_lte_direct_disc_parse_packed_msg
