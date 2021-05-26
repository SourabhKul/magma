/**
 * Copyright 2020 The Magma Authors.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "log.h"
#include "intertask_interface_types.h"
#include "intertask_interface.h"
#include "dynamic_memory_check.h"
#ifdef __cplusplus
}
#endif
#include "common_defs.h"
#include <sstream>
#include "amf_asDefs.h"
#include "amf_app_ue_context_and_proc.h"
#include "amf_authentication.h"
#include "amf_sap.h"

extern amf_config_t amf_config;
namespace magma5g {
extern task_zmq_ctx_s amf_app_task_zmq_ctx;
AmfMsg amf_msg_obj;
static int identification_t3570_handler(zloop_t* loop, int timer_id, void* arg);
int nas_proc_establish_ind(
    const amf_ue_ngap_id_t ue_id, const bool is_mm_ctx_new,
    const tai_t originating_tai, const ecgi_t ecgi,
    const m5g_rrc_establishment_cause_t as_cause, const s_tmsi_m5_t s_tmsi,
    bstring msg) {
  amf_sap_t amf_sap;
  uint32_t rc = RETURNerror;
  if (msg) {
    /*
     * Notify the AMF procedure call manager that NAS signalling
     * connection establishment indication message has been received
     * from the Access-Stratum sublayer
     */
    amf_sap.primitive                           = AMFAS_ESTABLISH_REQ;
    amf_sap.u.amf_as.primitive                  = _AMFAS_ESTABLISH_REQ;
    amf_sap.u.amf_as.u.establish.ue_id          = ue_id;
    amf_sap.u.amf_as.u.establish.is_initial     = true;
    amf_sap.u.amf_as.u.establish.is_amf_ctx_new = is_mm_ctx_new;
    amf_sap.u.amf_as.u.establish.nas_msg        = msg;
    amf_sap.u.amf_as.u.establish.ecgi           = ecgi;
    rc                                          = amf_sap_send(&amf_sap);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_AMF, rc);
}

/***************************************************************************
**                                                                        **
** Name:    nas_new_amf_procedures()                                     **
**                                                                        **
** Description: Generic function for new amf Procedures                   **
**                                                                        **
**                                                                        **
***************************************************************************/
amf_procedures_t* nas_new_amf_procedures(amf_context_t* const amf_context) {
  amf_procedures_t* amf_procedures = new amf_procedures_t;
  LIST_INIT(&amf_procedures->amf_common_procs);
  return amf_procedures;
}

/***************************************************************************
**                                                                        **
** Name:    nas5g_cn_auth_info_procedure()                            **
**                                                                        **
** Description: Generic function for new auth info  Procedure             **
**                                                                        **
**                                                                        **
***************************************************************************/
nas5g_auth_info_proc_t* nas5g_cn_auth_info_procedure(
    amf_context_t* const amf_context) {
  if (!(amf_context->amf_procedures)) {
    amf_context->amf_procedures = nas_new_amf_procedures(amf_context);
  }
  nas5g_auth_info_proc_t* auth_info_proc = new nas5g_auth_info_proc_t;
  auth_info_proc->cn_proc.base_proc.nas_puid =
      __sync_fetch_and_add(&nas_puid, 1);
  auth_info_proc->cn_proc.type  = CN5G_PROC_AUTH_INFO;
  nas5g_cn_procedure_t* wrapper = new nas5g_cn_procedure_t;
  if (wrapper) {
    wrapper->proc = &auth_info_proc->cn_proc;
    return auth_info_proc;
  } else {
    free_wrapper((void**) &auth_info_proc);
  }
  return NULL;
}

/***************************************************************************
**                                                                        **
** Name:    get_nas_specific_procedure_registration()                     **
**                                                                        **
** Description: Function for NAS Specific Procedure Registration          **
**                                                                        **
**                                                                        **
***************************************************************************/
nas_amf_registration_proc_t* get_nas_specific_procedure_registration(
    const amf_context_t* ctxt) {
  if ((ctxt) && (ctxt->amf_procedures) &&
      (ctxt->amf_procedures->amf_specific_proc) &&
      ((AMF_SPEC_PROC_TYPE_REGISTRATION ==
        ctxt->amf_procedures->amf_specific_proc->type))) {
    return (nas_amf_registration_proc_t*)
        ctxt->amf_procedures->amf_specific_proc;
  }
  return NULL;
}

/***************************************************************************
**                                                                        **
** Name:    is_nas_specific_procedure_registration_running()              **
**                                                                        **
** Description: Function to check if NAS procedure registration running   **
**                                                                        **
**                                                                        **
***************************************************************************/
bool is_nas_specific_procedure_registration_running(const amf_context_t* ctxt) {
  if ((ctxt) && (ctxt->amf_procedures) &&
      (ctxt->amf_procedures->amf_specific_proc) &&
      ((AMF_SPEC_PROC_TYPE_REGISTRATION ==
        ctxt->amf_procedures->amf_specific_proc->type)))
    return true;
  return false;
}

#if 0
/***************************************************************************
**                                                                        **
** Name:    nas5g_message_decode()                                        **
**                                                                        **
** Description: Invokes Function to decode NAS Message                    **
**                                                                        **
**                                                                        **
***************************************************************************/
int nas5g_message_decode(
    unsigned char* buffer, amf_nas_message_t* nas_msg, int length,
    amf_security_context_t* amf_security_context,
    amf_nas_message_decode_status_t* decode_status) {
  OAILOG_FUNC_IN(LOG_NAS5G);
  int bytes                  = 0;
  int size                   = 0;
  AmfMsgHeader_s* msg_header = NULL;
  AmfMsg* msg_amf            = NULL;
  /*
   * Decode the header
   */
  msg_header = (AmfMsgHeader_s*) &nas_msg->header;
  size       = amf_msg_obj.AmfMsgDecodeHeaderMsg(msg_header, buffer, length);
  if (size < 0) {
    OAILOG_FUNC_RETURN(LOG_NAS5G, TLV_BUFFER_TOO_SHORT);
  }
  /*
   * Decode plain NAS message
   */
  msg_amf = (AmfMsg*) &nas_msg->plain.amf;
  bytes   = amf_msg_obj.M5gNasMessageDecodeMsg(msg_amf, buffer, length);
  OAILOG_FUNC_RETURN(LOG_NAS, bytes);
}
#endif

/***************************************************************************
**                                                                        **
** Name:    nas5g_message_decode()                                        **
**                                                                        **
** Description: Invokes Function to decode NAS Message                    **
**                                                                        **
**                                                                        **
***************************************************************************/
nas_amf_ident_proc_t* nas5g_new_identification_procedure(
    amf_context_t* const amf_context) {
  if (!(amf_context->amf_procedures)) {
    amf_context->amf_procedures = nas_new_amf_procedures(amf_context);
  }
  nas_amf_ident_proc_t* ident_proc = new nas_amf_ident_proc_t;
  ident_proc->amf_com_proc.amf_proc.base_proc.nas_puid =
      __sync_fetch_and_add(&nas_puid, 1);
  ident_proc->amf_com_proc.amf_proc.type = NAS_AMF_PROC_TYPE_COMMON;
  ident_proc->T3570.sec                  = amf_config.nas_config.t3570_sec;
  ident_proc->T3570.id                   = AMF_APP_TIMER_INACTIVE_ID;
  ident_proc->amf_com_proc.type          = AMF_COMM_PROC_IDENT;
  nas_amf_common_procedure_t* wrapper    = new nas_amf_common_procedure_t;
  if (wrapper) {
    wrapper->proc = &ident_proc->amf_com_proc;
    LIST_INSERT_HEAD(
        &amf_context->amf_procedures->amf_common_procs, wrapper, entries);
    return ident_proc;
  } else {
    free_wrapper((void**) &ident_proc);
  }
  return ident_proc;
}

/***************************************************************************
** Description: Sends IDENTITY REQUEST message.                           **
**                                                                        **
** Inputs:  args:      handler parameters                                 **
**      Others:    None                                                   **
**                                                                        **
** Outputs:        None                                                   **
**      Return:    None                                                   **
***************************************************************************/
static int amf_identification_request(nas_amf_ident_proc_t* const proc) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  amf_sap_t amf_sap;  //             = {0};
  int rc         = RETURNok;
  proc->T3570.id = NAS5G_TIMER_INACTIVE_ID;
  OAILOG_DEBUG(LOG_AMF_APP, "Sending AS IDENTITY_REQUEST\n");
  /*
   * Notify AMF-AS SAP that Identity Request message has to be sent
   * to the UE
   */
  amf_sap.primitive = AMFAS_SECURITY_REQ;
  amf_sap.u.amf_as.u.security.puid =
      proc->amf_com_proc.amf_proc.base_proc.nas_puid;
  amf_sap.u.amf_as.u.security.ue_id                    = proc->ue_id;
  amf_sap.u.amf_as.u.security.msg_type                 = AMF_AS_MSG_TYPE_IDENT;
  amf_sap.u.amf_as.u.security.ident_type               = proc->identity_type;
  amf_sap.u.amf_as.u.security.sctx.is_knas_int_present = true;
  amf_sap.u.amf_as.u.security.sctx.is_knas_enc_present = true;
  amf_sap.u.amf_as.u.security.sctx.is_new              = true;
  rc                                                   = amf_sap_send(&amf_sap);

  if (rc != RETURNerror) {
    /*
     * Start Identification T3570 timer
     */
    // TODO
    OAILOG_INFO(
        LOG_AMF_APP, "AMF_TEST: Timer: Starting Identity timer T3570 \n");
    proc->T3570.id = start_timer(
        &amf_app_task_zmq_ctx, IDENTITY_TIMER_EXPIRY_MSECS, TIMER_REPEAT_ONCE,
        identification_t3570_handler, (void*) proc->ue_id);
    OAILOG_INFO(
        LOG_AMF_APP, "Timer: Started Identity timer T3570 with id %d\n",
        proc->T3570.id);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_AMF, rc);
}

/* Identification Timer T3570 Expiry Handler */
static int identification_t3570_handler(
    zloop_t* loop, int timer_id, void* arg) {
#if 0  /* TIMER_CHANGES_REVIEW */
  amf_ue_ngap_id_t ue_id = NULL;
  ue_id                  = *((amf_ue_ngap_id_t*) (arg));
  amf_context_t* amf_ctx = NULL;
  OAILOG_INFO(LOG_AMF_APP, "Timer: identification T3570 handler \n");

  ue_m5gmm_context_s* ue_context = amf_ue_context_exists_amf_ue_ngap_id(ue_id);

  if (ue_context == NULL) {
    OAILOG_INFO(LOG_AMF_APP, "AMF_TEST: ue_context is NULL\n");
    return -1;
  }

  amf_ctx = &ue_context->amf_context;
  if (!(amf_ctx)) {
    OAILOG_ERROR(LOG_AMF_APP, "Timer: T3570 timer expired No AMF context\n");
    return 1;
  }

  nas_amf_ident_proc_t* ident_proc =
      get_5g_nas_common_procedure_identification(amf_ctx);

  if (ident_proc) {
    OAILOG_WARNING(
        LOG_AMF_APP, "T3570 timer   timer id %d ue id %d\n",
        ident_proc->T3570.id, ident_proc->ue_id);
    ident_proc->T3570.id = NAS5G_TIMER_INACTIVE_ID;
  }
  /*
   * Increment the retransmission counter
   */
  ident_proc->retransmission_count += 1;
  OAILOG_ERROR(
      LOG_AMF_APP, "Timer: Incrementing retransmission_count to %d\n",
      ident_proc->retransmission_count);

  if (ident_proc->retransmission_count < IDENTIFICATION_COUNTER_MAX) {
    /*
     * Send identity request message to the UE
     */
    OAILOG_ERROR(
        LOG_AMF_APP, "Timer: IDENTIFICATION_COUNTER_MAX =  %d\n",
        IDENTIFICATION_COUNTER_MAX);
    OAILOG_ERROR(
        LOG_AMF_APP,
        "Timer: timer has expired Sending Identification request again\n");
    amf_identification_request(ident_proc);
  } else {
    /*
     * Abort the identification procedure
     */
    OAILOG_ERROR(
        LOG_AMF_APP,
        "Timer: Maximum retires done hence Abort the identification "
        "procedure\n");
    return -1;
  }
#endif /*  TIMER_CHANGES_REVIEW */
  return 0;
}

//-------------------------------------------------------------------------------------
int amf_proc_identification(
    amf_context_t* const amf_context, nas_amf_proc_t* const amf_proc,
    const identity_type2_t type, success_cb_t success, failure_cb_t failure) {
  OAILOG_FUNC_IN(LOG_NAS_AMF);
  int rc                     = RETURNerror;
  amf_context->amf_fsm_state = AMF_REGISTERED;
  if ((amf_context) && ((AMF_DEREGISTERED == amf_context->amf_fsm_state) ||
                        (AMF_REGISTERED == amf_context->amf_fsm_state))) {
    amf_ue_ngap_id_t ue_id =
        PARENT_STRUCT(amf_context, ue_m5gmm_context_s, amf_context)
            ->amf_ue_ngap_id;
    nas_amf_ident_proc_t* ident_proc =
        nas5g_new_identification_procedure(amf_context);
    if (ident_proc) {
      if (amf_proc) {
        if ((NAS_AMF_PROC_TYPE_SPECIFIC == amf_proc->type) &&
            (AMF_SPEC_PROC_TYPE_REGISTRATION ==
             ((nas_amf_specific_proc_t*) amf_proc)->type)) {
          ident_proc->is_cause_is_registered = true;
        }
      }
      ident_proc->identity_type                                 = type;
      ident_proc->retransmission_count                          = 0;
      ident_proc->ue_id                                         = ue_id;
      ident_proc->amf_com_proc.amf_proc.delivered               = NULL;
      ident_proc->amf_com_proc.amf_proc.base_proc.success_notif = success;
      ident_proc->amf_com_proc.amf_proc.base_proc.failure_notif = failure;
      ident_proc->amf_com_proc.amf_proc.base_proc.fail_in       = NULL;
    }
    rc = amf_identification_request(ident_proc);

    if (rc != RETURNerror) {
      /*
       * Notify 5G CN that common procedure has been initiated
       */
      amf_sap_t amf_sap;
      amf_sap.primitive       = AMFREG_COMMON_PROC_REQ;
      amf_sap.u.amf_reg.ue_id = ue_id;
      amf_sap.u.amf_reg.ctx   = amf_context;
      rc                      = amf_sap_send(&amf_sap);
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_AMF, rc);
}
}  // namespace magma5g
