#include "sip_calling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define THIS_FILE "sip_calling"

// Module state
static sip_call_config_t config;
static pjsip_endpoint* sip_endpt = NULL;
static pj_pool_t* pool = NULL;
static sip_call_session_t current_session;
static sip_call_state_callback_t state_callback = NULL;
static sip_call_audio_callback_t audio_callback = NULL;
static void* callback_user_data = NULL;

// Forward declarations
static void set_call_state(sip_call_state_t new_state);
static pj_status_t send_invite_request(void);
static pj_status_t send_ack_request(pjsip_rx_data* rdata);
static pj_status_t send_bye_request(void);
static void clear_session_data(void);
static void store_dialog_info_from_invite(pjsip_rx_data *rdata);
static void store_dialog_info_from_response(pjsip_rx_data *rdata);
static int parse_h264_payload_type(const char *sdp_content);
static int parse_telephone_event_payload_type(const char *sdp_content);

pj_status_t sip_calling_init(const sip_call_config_t* call_config,
                            pjsip_endpoint* endpt,
                            pj_pool_t* mem_pool) {
    if (!call_config || !endpt || !mem_pool) {
        return PJ_EINVAL;
    }

    // Store configuration
    memcpy(&config, call_config, sizeof(sip_call_config_t));
    sip_endpt = endpt;
    pool = mem_pool;

    // Initialize session
    memset(&current_session, 0, sizeof(sip_call_session_t));
    current_session.state = SIP_CALL_STATE_IDLE;
    current_session.direction = SIP_CALL_DIRECTION_NONE;
    current_session.remote_rtp_port = 8000;  // Default
    current_session.remote_dtmf_payload_type = 101;
    current_session.remote_video_rtp_port = 0;
    current_session.remote_video_payload_type = 0;

    PJ_LOG(3,(THIS_FILE, "Unified SIP calling initialized - target: %s, timeout: %ds",
              config.target_uri, config.call_timeout_seconds));

    return PJ_SUCCESS;
}

void sip_calling_set_callbacks(sip_call_state_callback_t state_cb,
                              sip_call_audio_callback_t audio_cb,
                              void* user_data) {
    state_callback = state_cb;
    audio_callback = audio_cb;
    callback_user_data = user_data;
}

static void set_call_state(sip_call_state_t new_state) {
    sip_call_state_t old_state = current_session.state;

    if (old_state != new_state) {
        current_session.state = new_state;

        PJ_LOG(3,(THIS_FILE, "Call state changed: %d -> %d (direction: %d)",
                  old_state, new_state, current_session.direction));

        // Update timing
        if (new_state == SIP_CALL_STATE_CALLING) {
            current_session.call_start_time = time(NULL);
        } else if (new_state == SIP_CALL_STATE_IDLE) {
            // Clear all session data when going to idle
            clear_session_data();
        } else if (new_state == SIP_CALL_STATE_FAILED) {
            // Auto-reset to IDLE after brief delay for failed calls
            PJ_LOG(2,(THIS_FILE, "Call failed - resetting to IDLE"));
            clear_session_data();
            current_session.state = SIP_CALL_STATE_IDLE;  // Override the state change
            new_state = SIP_CALL_STATE_IDLE;  // Update for callback
        } else if (new_state == SIP_CALL_STATE_ESTABLISHED) {
            // Trigger audio callback when call is established
            if (audio_callback) {
                audio_callback(current_session.remote_ip,
                              current_session.remote_rtp_port,
                              current_session.remote_video_rtp_port,
                              callback_user_data);
            }
        }

        // Notify callback
        if (state_callback) {
            state_callback(old_state, new_state, callback_user_data);
        }
    }
}

static void clear_session_data(void) {
    current_session.call_start_time = 0;
    current_session.direction = SIP_CALL_DIRECTION_NONE;
    current_session.remote_rtp_port = 8000;
    current_session.remote_dtmf_payload_type = 101;
    current_session.remote_video_rtp_port = 0;
    current_session.remote_video_payload_type = 0;
    current_session.invite_cseq = 0;

    // Clear all string data
    memset(current_session.call_id_buf, 0, sizeof(current_session.call_id_buf));
    memset(current_session.local_tag_buf, 0, sizeof(current_session.local_tag_buf));
    memset(current_session.remote_tag_buf, 0, sizeof(current_session.remote_tag_buf));
    memset(current_session.remote_contact_buf, 0, sizeof(current_session.remote_contact_buf));
    memset(current_session.remote_uri_buf, 0, sizeof(current_session.remote_uri_buf));
    memset(current_session.remote_ip, 0, sizeof(current_session.remote_ip));

    // Reset pj_str_t references
    current_session.call_id.slen = 0;
    current_session.local_tag.slen = 0;
    current_session.remote_tag.slen = 0;
    current_session.remote_contact.slen = 0;
    current_session.remote_uri.slen = 0;
}

pj_status_t sip_calling_make_call(void) {
    if (current_session.state != SIP_CALL_STATE_IDLE) {
        PJ_LOG(2,(THIS_FILE, "Cannot make call - already in state %d", current_session.state));
        return PJ_EBUSY;
    }

    PJ_LOG(3,(THIS_FILE, "Initiating outgoing call to %s", config.target_uri));

    current_session.direction = SIP_CALL_DIRECTION_OUTGOING;
    set_call_state(SIP_CALL_STATE_CALLING);

    return send_invite_request();
}

static void store_dialog_info_from_invite(pjsip_rx_data *rdata) {
    // Store Call-ID
    pjsip_cid_hdr *cid_hdr = (pjsip_cid_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
    if (cid_hdr) {
        int len = (cid_hdr->id.slen < sizeof(current_session.call_id_buf) - 1) ?
                  cid_hdr->id.slen : sizeof(current_session.call_id_buf) - 1;
        memcpy(current_session.call_id_buf, cid_hdr->id.ptr, len);
        current_session.call_id_buf[len] = '\0';
        current_session.call_id.ptr = current_session.call_id_buf;
        current_session.call_id.slen = len;
        PJ_LOG(3,(THIS_FILE, "Stored Call-ID: %.*s", current_session.call_id.slen, current_session.call_id.ptr));
    }

    // Store remote tag from From header (their tag)
    pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_FROM, NULL);
    if (from_hdr) {
        if (from_hdr->tag.slen > 0) {
            int len = (from_hdr->tag.slen < sizeof(current_session.remote_tag_buf) - 1) ?
                      from_hdr->tag.slen : sizeof(current_session.remote_tag_buf) - 1;
            memcpy(current_session.remote_tag_buf, from_hdr->tag.ptr, len);
            current_session.remote_tag_buf[len] = '\0';
            current_session.remote_tag.ptr = current_session.remote_tag_buf;
            current_session.remote_tag.slen = len;
            PJ_LOG(3,(THIS_FILE, "Stored remote tag: %.*s", current_session.remote_tag.slen, current_session.remote_tag.ptr));
        }

        // Store remote URI from From header
        char uri_buf[128];
        int uri_len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, from_hdr->uri, uri_buf, sizeof(uri_buf));
        if (uri_len > 0) {
            uri_buf[uri_len] = '\0';
            int len = (uri_len < sizeof(current_session.remote_uri_buf) - 1) ?
                      uri_len : sizeof(current_session.remote_uri_buf) - 1;
            memcpy(current_session.remote_uri_buf, uri_buf, len);
            current_session.remote_uri_buf[len] = '\0';
            current_session.remote_uri.ptr = current_session.remote_uri_buf;
            current_session.remote_uri.slen = len;
            PJ_LOG(3,(THIS_FILE, "Stored remote URI: %.*s", current_session.remote_uri.slen, current_session.remote_uri.ptr));
        }
    }

    // Store remote contact for routing
    pjsip_contact_hdr *contact_hdr = (pjsip_contact_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL);
    if (contact_hdr && contact_hdr->uri) {
        char contact_buf[128];
        int contact_len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, contact_hdr->uri, contact_buf, sizeof(contact_buf));
        if (contact_len > 0) {
            contact_buf[contact_len] = '\0';
            int len = (contact_len < sizeof(current_session.remote_contact_buf) - 1) ?
                      contact_len : sizeof(current_session.remote_contact_buf) - 1;
            memcpy(current_session.remote_contact_buf, contact_buf, len);
            current_session.remote_contact_buf[len] = '\0';
            current_session.remote_contact.ptr = current_session.remote_contact_buf;
            current_session.remote_contact.slen = len;
            PJ_LOG(3,(THIS_FILE, "Stored remote contact: %.*s", current_session.remote_contact.slen, current_session.remote_contact.ptr));
        }
    }

    // Extract remote IP for RTP
    strcpy(current_session.remote_ip, pj_inet_ntoa(rdata->pkt_info.src_addr.ipv4.sin_addr));
}

static void store_dialog_info_from_response(pjsip_rx_data *rdata) {
    // Store remote tag from To header (their tag)
    pjsip_to_hdr *to_hdr = (pjsip_to_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_TO, NULL);
    if (to_hdr && to_hdr->tag.slen > 0) {
        int len = (to_hdr->tag.slen < sizeof(current_session.remote_tag_buf) - 1) ?
                  to_hdr->tag.slen : sizeof(current_session.remote_tag_buf) - 1;
        memcpy(current_session.remote_tag_buf, to_hdr->tag.ptr, len);
        current_session.remote_tag_buf[len] = '\0';
        current_session.remote_tag.ptr = current_session.remote_tag_buf;
        current_session.remote_tag.slen = len;
        PJ_LOG(3,(THIS_FILE, "Stored remote tag from response: %.*s", current_session.remote_tag.slen, current_session.remote_tag.ptr));
    }

    // Store remote contact for BYE routing
    pjsip_contact_hdr *contact_hdr = (pjsip_contact_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL);
    if (contact_hdr && contact_hdr->uri) {
        char contact_buf[128];
        int len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, contact_hdr->uri, contact_buf, sizeof(contact_buf));
        if (len > 0) {
            contact_buf[len] = '\0';
            int buf_len = (len < sizeof(current_session.remote_contact_buf) - 1) ?
                          len : sizeof(current_session.remote_contact_buf) - 1;
            memcpy(current_session.remote_contact_buf, contact_buf, buf_len);
            current_session.remote_contact_buf[buf_len] = '\0';
            current_session.remote_contact.ptr = current_session.remote_contact_buf;
            current_session.remote_contact.slen = buf_len;
        }
    }

    // Extract remote IP for RTP
    strcpy(current_session.remote_ip, pj_inet_ntoa(rdata->pkt_info.src_addr.ipv4.sin_addr));
}

pj_status_t sip_calling_handle_incoming_invite(pjsip_rx_data *rdata) {
    pjsip_tx_data *tdata;
    pj_status_t status;
    char sdp_body[512];
    pj_str_t sdp_str;
    pjsip_msg_body *body;
    int remote_rtp_port = 8000;
    int remote_dtmf_payload_type = 101;
    int remote_video_rtp_port = 0;
    int remote_video_payload_type = 0;
    char local_tag_buf[32];

    PJ_LOG(3,(THIS_FILE, "=== HANDLING INCOMING INVITE ==="));

    // Check if we're already in a call
    if (current_session.state != SIP_CALL_STATE_IDLE) {
        // Check if this is a re-INVITE for an existing call
        pjsip_cid_hdr *cid_hdr = (pjsip_cid_hdr*)
            pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
        if (cid_hdr && current_session.call_id.slen > 0 &&
            pj_strcmp(&cid_hdr->id, &current_session.call_id) == 0) {
            PJ_LOG(3,(THIS_FILE, "Received re-INVITE for established call"));

            // Create proper 200 OK response with SDP for re-INVITE
            status = pjsip_endpt_create_response(sip_endpt, rdata, 200, NULL, &tdata);
            if (status == PJ_SUCCESS) {
                // Add SDP body (reuse SDP creation logic)
                status = sip_calling_create_sdp_offer(tdata->pool, config.local_ip,
                                                     config.local_rtp_port,
                                                     config.local_video_rtp_port,
                                                     current_session.remote_video_payload_type ?
                                                     current_session.remote_video_payload_type :
                                                     config.video_payload_type,
                                                     &sdp_str);
                if (status == PJ_SUCCESS) {
                    pj_str_t content_type = pj_str("application");
                    pj_str_t content_subtype = pj_str("sdp");
                    body = pjsip_msg_body_create(tdata->pool, &content_type, &content_subtype, &sdp_str);
                    tdata->msg->body = body;
                }
                pjsip_endpt_send_response2(sip_endpt, rdata, tdata, NULL, NULL);
            } else {
                pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);
            }
            return PJ_SUCCESS;
        } else {
            // Different call - check if we should prioritize incoming call
            if (current_session.state == SIP_CALL_STATE_CALLING ||
                current_session.state == SIP_CALL_STATE_RINGING) {
                // Outgoing call not yet answered - prioritize incoming call
                PJ_LOG(3,(THIS_FILE, "Incoming call received while outgoing call in progress (state %d) - prioritizing incoming call", current_session.state));

                // Send CANCEL to terminate outgoing call if in CALLING/RINGING state
                if (current_session.direction == SIP_CALL_DIRECTION_OUTGOING) {
                    PJ_LOG(3,(THIS_FILE, "Terminating outgoing call to accept incoming call"));
                    // Note: We don't send BYE here because the call isn't established yet
                    // The outgoing call will be terminated by going to IDLE state
                }

                // Reset session for incoming call
                set_call_state(SIP_CALL_STATE_IDLE);
                // Fall through to handle incoming call normally
            } else {
                // Call is established or in other states - reject incoming call
                PJ_LOG(2,(THIS_FILE, "Incoming call rejected - already in established call (state %d)", current_session.state));
                pjsip_endpt_respond_stateless(sip_endpt, rdata, 486, NULL, NULL, NULL); // Busy Here
                return PJ_EBUSY;
            }
        }
    }

    // Set up for incoming call (same logic as before)
    current_session.direction = SIP_CALL_DIRECTION_INCOMING;

    // ... rest of the function remains the same
    // Store dialog information from the INVITE
    store_dialog_info_from_invite(rdata);

    // Parse incoming SDP to get remote RTP port
    if (rdata->msg_info.msg->body && rdata->msg_info.msg->body->data) {
        PJ_LOG(3,(THIS_FILE, "SDP body found, length: %d", rdata->msg_info.msg->body->len));
        char *sdp_content = (char*)rdata->msg_info.msg->body->data;
        PJ_LOG(3,(THIS_FILE, "Remote SDP:\n%.*s",
                  (int)rdata->msg_info.msg->body->len, sdp_content));
        sip_calling_parse_sdp_answer(sdp_content, &remote_rtp_port,
                                     &remote_dtmf_payload_type,
                                     &remote_video_rtp_port,
                                     &remote_video_payload_type);
        current_session.remote_rtp_port = remote_rtp_port;
        current_session.remote_dtmf_payload_type = remote_dtmf_payload_type;
        current_session.remote_video_rtp_port = remote_video_rtp_port;
        current_session.remote_video_payload_type = remote_video_payload_type;
        PJ_LOG(3,(THIS_FILE, "Parsed remote RTP: audio=%d dtmf_payload=%d video=%d h264_payload=%d",
                  remote_rtp_port, remote_dtmf_payload_type,
                  remote_video_rtp_port, remote_video_payload_type));
    }

    // Generate our local tag
    snprintf(local_tag_buf, sizeof(local_tag_buf), "wibox-%ld", time(NULL));
    strcpy(current_session.local_tag_buf, local_tag_buf);
    current_session.local_tag.ptr = current_session.local_tag_buf;
    current_session.local_tag.slen = strlen(local_tag_buf);

    // Create 200 OK response
    status = pjsip_endpt_create_response(sip_endpt, rdata, 200, NULL, &tdata);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create 200 OK response: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    // Set our To tag in the response
    pjsip_to_hdr *resp_to_hdr = (pjsip_to_hdr*)
        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TO, NULL);
    if (resp_to_hdr) {
        pj_str_t to_tag_str = pj_str(local_tag_buf);
        pj_strdup(tdata->pool, &resp_to_hdr->tag, &to_tag_str);
    }

    // Add Contact header
    pjsip_contact_hdr *contact_hdr;
    pj_str_t contact_uri;
    char contact_str[64];
    snprintf(contact_str, sizeof(contact_str), "<sip:wibox@%s:%d>",
             config.local_ip, config.local_sip_port);
    pj_strdup2(tdata->pool, &contact_uri, contact_str);
    contact_hdr = pjsip_contact_hdr_create(tdata->pool);
    contact_hdr->uri = pjsip_parse_uri(tdata->pool, contact_uri.ptr, contact_uri.slen, 0);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)contact_hdr);

    // Create and add SDP answer
    status = sip_calling_create_sdp_offer(tdata->pool, config.local_ip,
                                         config.local_rtp_port,
                                         config.local_video_rtp_port,
                                         current_session.remote_video_payload_type ?
                                         current_session.remote_video_payload_type :
                                         config.video_payload_type,
                                         &sdp_str);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create SDP answer: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    // Add SDP body
    pj_str_t content_type = pj_str("application");
    pj_str_t content_subtype = pj_str("sdp");
    body = pjsip_msg_body_create(tdata->pool, &content_type, &content_subtype, &sdp_str);
    tdata->msg->body = body;

    // Add Content-Type header
    pjsip_generic_string_hdr *ct_hdr;
    pj_str_t ct_name = pj_str("Content-Type");
    pj_str_t ct_value = pj_str("application/sdp");
    ct_hdr = pjsip_generic_string_hdr_create(tdata->pool, &ct_name, &ct_value);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)ct_hdr);

    // Send 200 OK response
    status = pjsip_endpt_send_response2(sip_endpt, rdata, tdata, NULL, NULL);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to send 200 OK response: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    PJ_LOG(3,(THIS_FILE, "200 OK response sent for incoming call"));

    // Move to INCOMING state (waiting for ACK)
    set_call_state(SIP_CALL_STATE_INCOMING);

    return PJ_SUCCESS;
}

pj_status_t sip_calling_handle_incoming_ack(pjsip_rx_data *rdata) {
    PJ_LOG(3,(THIS_FILE, "Received ACK for incoming call"));

    // Verify this ACK is for our current call
    pjsip_cid_hdr *cid_hdr = (pjsip_cid_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
    if (!cid_hdr || pj_strcmp(&cid_hdr->id, &current_session.call_id) != 0) {
        PJ_LOG(2,(THIS_FILE, "ACK Call-ID doesn't match current call"));
        return PJ_EINVAL;
    }

    if (current_session.state != SIP_CALL_STATE_INCOMING) {
        PJ_LOG(2,(THIS_FILE, "Received ACK but not in INCOMING state (state: %d)", current_session.state));
        return PJ_EINVAL;
    }

    // Call is now established
    set_call_state(SIP_CALL_STATE_ESTABLISHED);

    return PJ_SUCCESS;
}

pj_status_t sip_calling_handle_incoming_bye(pjsip_rx_data *rdata) {
    PJ_LOG(3,(THIS_FILE, "Received BYE request"));

    // Verify this BYE is for our current call
    pjsip_cid_hdr *cid_hdr = (pjsip_cid_hdr*)
        pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
    if (!cid_hdr || current_session.call_id.slen == 0 ||
        pj_strcmp(&cid_hdr->id, &current_session.call_id) != 0) {
        PJ_LOG(2,(THIS_FILE, "BYE Call-ID doesn't match current call"));
        pjsip_endpt_respond_stateless(sip_endpt, rdata, 481, NULL, NULL, NULL); // Call/Transaction Does Not Exist
        return PJ_EINVAL;
    }

    // Send 200 OK response to BYE
    pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);

    // Terminate the call
    set_call_state(SIP_CALL_STATE_IDLE);

    return PJ_SUCCESS;
}

pj_status_t sip_calling_handle_incoming_cancel(pjsip_rx_data *rdata) {
    PJ_LOG(3,(THIS_FILE, "Received CANCEL request"));

    // Send 200 OK response to CANCEL
    pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);

    // Terminate the call if it's in INCOMING state
    if (current_session.state == SIP_CALL_STATE_INCOMING) {
        set_call_state(SIP_CALL_STATE_IDLE);
    }

    return PJ_SUCCESS;
}

static pj_status_t send_invite_request(void) {
    pjsip_tx_data *tdata;
    pj_status_t status;
    pj_str_t target_uri_str;
    pj_str_t from_uri_str;
    pj_str_t sdp_str;
    pjsip_msg_body *body;
    char from_uri_buf[128];
    char call_id_buf[64];

    // Create URI strings
    target_uri_str = pj_str(config.target_uri);

    snprintf(from_uri_buf, sizeof(from_uri_buf), "sip:caller@%s:%d",
             config.local_ip, config.local_sip_port);
    from_uri_str = pj_str(from_uri_buf);

    // Generate Call-ID
    snprintf(call_id_buf, sizeof(call_id_buf), "call-%ld@%s",
             time(NULL), config.local_ip);
    pj_str_t call_id_str = pj_str(call_id_buf);

    // Store Call-ID
    strcpy(current_session.call_id_buf, call_id_buf);
    current_session.call_id = pj_str(current_session.call_id_buf);

    // Create INVITE request
    status = pjsip_endpt_create_request(sip_endpt, pjsip_get_invite_method(),
                                       &target_uri_str, &from_uri_str, &target_uri_str,
                                       NULL, &call_id_str, -1, NULL, &tdata);

    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create INVITE request: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    // Capture the CSeq that PJSIP generated for the INVITE
    pjsip_cseq_hdr *cseq_hdr = (pjsip_cseq_hdr*)
        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    if (cseq_hdr) {
        current_session.invite_cseq = cseq_hdr->cseq;
        PJ_LOG(3,(THIS_FILE, "INVITE CSeq: %d", current_session.invite_cseq));
    }

    // Generate and set our local tag (From tag for outgoing call)
    char local_tag_buf[32];
    snprintf(local_tag_buf, sizeof(local_tag_buf), "caller-%ld", time(NULL));

    pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)
        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
    if (from_hdr) {
        pj_str_t from_tag_str = pj_str(local_tag_buf);
        pj_strdup(tdata->pool, &from_hdr->tag, &from_tag_str);

        // Store our local tag
        strcpy(current_session.local_tag_buf, local_tag_buf);
        current_session.local_tag = pj_str(current_session.local_tag_buf);
    }

    // Add Contact header
    pjsip_contact_hdr *contact_hdr;
    pj_str_t contact_uri;
    char contact_str[128];
    snprintf(contact_str, sizeof(contact_str), "<sip:caller@%s:%d>",
             config.local_ip, config.local_sip_port);
    pj_strdup2(tdata->pool, &contact_uri, contact_str);
    contact_hdr = pjsip_contact_hdr_create(tdata->pool);
    contact_hdr->uri = pjsip_parse_uri(tdata->pool, contact_uri.ptr, contact_uri.slen, 0);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)contact_hdr);

    // Create and add SDP offer
    status = sip_calling_create_sdp_offer(tdata->pool, config.local_ip,
                                         config.local_rtp_port,
                                         config.local_video_rtp_port,
                                         config.video_payload_type,
                                         &sdp_str);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create SDP offer: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    // Add SDP body to INVITE
    pj_str_t content_type = pj_str("application");
    pj_str_t content_subtype = pj_str("sdp");
    body = pjsip_msg_body_create(tdata->pool, &content_type, &content_subtype, &sdp_str);
    tdata->msg->body = body;

    // Add Content-Type header
    pjsip_generic_string_hdr *ct_hdr;
    pj_str_t ct_name = pj_str("Content-Type");
    pj_str_t ct_value = pj_str("application/sdp");
    ct_hdr = pjsip_generic_string_hdr_create(tdata->pool, &ct_name, &ct_value);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)ct_hdr);

    // Send INVITE
    status = pjsip_endpt_send_request_stateless(sip_endpt, tdata, NULL, NULL);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to send INVITE request: %d", status));
        set_call_state(SIP_CALL_STATE_FAILED);
        return status;
    }

    PJ_LOG(3,(THIS_FILE, "INVITE request sent to %s", config.target_uri));
    return PJ_SUCCESS;
}

static pj_status_t send_bye_request(void) {
    pjsip_tx_data *tdata;
    pj_status_t status;
    pjsip_method bye_method = *pjsip_get_bye_method();
    pj_str_t from_uri_str;
    pj_str_t to_uri_str;
    char from_uri_buf[128];
    pj_str_t *bye_target;
    int retry_count;

    PJ_LOG(3,(THIS_FILE, "Sending BYE request"));

    if (current_session.call_id.slen == 0) {
        PJ_LOG(1,(THIS_FILE, "No Call-ID stored, cannot send BYE"));
        return PJ_EINVAL;
    }

    // Use Contact header if available for routing, otherwise use target/remote URI
    if (current_session.remote_contact.slen > 0) {
        bye_target = &current_session.remote_contact;
    } else if (current_session.direction == SIP_CALL_DIRECTION_OUTGOING) {
        bye_target = &(pj_str_t){config.target_uri, strlen(config.target_uri)};
    } else {
        bye_target = &current_session.remote_uri;
    }

    // Set up From/To URIs based on call direction
    if (current_session.direction == SIP_CALL_DIRECTION_OUTGOING) {
        // For outgoing calls: we are From, they are To
        snprintf(from_uri_buf, sizeof(from_uri_buf), "sip:caller@%s:%d",
                 config.local_ip, config.local_sip_port);
        from_uri_str = pj_str(from_uri_buf);
        to_uri_str = pj_str(config.target_uri);
    } else {
        // For incoming calls: we are To, they are From
        snprintf(from_uri_buf, sizeof(from_uri_buf), "sip:wibox@%s:%d",
                 config.local_ip, config.local_sip_port);
        from_uri_str = pj_str(from_uri_buf);
        to_uri_str = current_session.remote_uri;
    }

    // Send BYE multiple times for reliability (UDP can drop packets)
    for (retry_count = 0; retry_count < 3; retry_count++) {
        status = pjsip_endpt_create_request(sip_endpt, &bye_method,
                                           bye_target, &from_uri_str, &to_uri_str,
                                           NULL, &current_session.call_id, -1, NULL, &tdata);

        if (status != PJ_SUCCESS) {
            PJ_LOG(1,(THIS_FILE, "Failed to create BYE request #%d: %d", retry_count + 1, status));
            continue;
        }

        // Set From tag (our tag)
        pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)
            pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
        if (from_hdr && current_session.local_tag.slen > 0) {
            pj_strdup(tdata->pool, &from_hdr->tag, &current_session.local_tag);
        }

        // Set To tag (their tag)
        pjsip_to_hdr *to_hdr = (pjsip_to_hdr*)
            pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TO, NULL);
        if (to_hdr && current_session.remote_tag.slen > 0) {
            pj_strdup(tdata->pool, &to_hdr->tag, &current_session.remote_tag);
        }

        // Add Contact header
        pjsip_contact_hdr *contact_hdr;
        pj_str_t contact_uri;
        char contact_str[64];
        snprintf(contact_str, sizeof(contact_str), "<sip:wibox@%s:%d>",
                 config.local_ip, config.local_sip_port);
        pj_strdup2(tdata->pool, &contact_uri, contact_str);
        contact_hdr = pjsip_contact_hdr_create(tdata->pool);
        contact_hdr->uri = pjsip_parse_uri(tdata->pool, contact_uri.ptr, contact_uri.slen, 0);
        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)contact_hdr);

        // Send BYE
        status = pjsip_endpt_send_request_stateless(sip_endpt, tdata, NULL, NULL);
        if (status == PJ_SUCCESS) {
            PJ_LOG(3,(THIS_FILE, "BYE request #%d sent successfully", retry_count + 1));
        } else {
            PJ_LOG(1,(THIS_FILE, "Failed to send BYE request #%d: %d", retry_count + 1, status));
        }

        // Small delay between retries
        if (retry_count < 2) {
            usleep(50000); // 50ms delay
        }
    }

    return PJ_SUCCESS;
}

pj_status_t sip_calling_terminate_call(void) {
    if (current_session.state == SIP_CALL_STATE_IDLE) {
        return PJ_SUCCESS;  // Already idle
    }

    PJ_LOG(3,(THIS_FILE, "Terminating call (direction: %d, state: %d)",
              current_session.direction, current_session.state));

    if (current_session.state == SIP_CALL_STATE_ESTABLISHED) {
        // Send BYE for established calls (both incoming and outgoing)
        send_bye_request();
    }

    set_call_state(SIP_CALL_STATE_IDLE);
    return PJ_SUCCESS;
}

static pj_status_t send_ack_request(pjsip_rx_data* rdata) {
    pjsip_tx_data *tdata;
    pj_status_t status;
    pjsip_method ack_method = *pjsip_get_ack_method();
    pj_str_t from_uri_str;
    pj_str_t target_uri_str;
    char from_uri_buf[128];

    snprintf(from_uri_buf, sizeof(from_uri_buf), "sip:caller@%s:%d",
             config.local_ip, config.local_sip_port);
    from_uri_str = pj_str(from_uri_buf);
    target_uri_str = pj_str(config.target_uri);

    // Use Contact URI from 200 OK for ACK Request-URI
    pj_str_t *ack_target = &current_session.remote_contact;
    if (current_session.remote_contact.slen == 0) {
        ack_target = &target_uri_str;
    }

    // Use the SAME CSeq as the original INVITE
    status = pjsip_endpt_create_request(sip_endpt, &ack_method,
                                       ack_target,
                                       &from_uri_str,
                                       &target_uri_str,
                                       NULL,
                                       &current_session.call_id,
                                       current_session.invite_cseq,  // SAME CSeq as INVITE
                                       NULL, &tdata);

    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create ACK request: %d", status));
        return status;
    }

    // Set From tag (our tag)
    pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)
        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
    if (from_hdr) {
        pj_strdup(tdata->pool, &from_hdr->tag, &current_session.local_tag);
    }

    // Set To tag (their tag from 200 OK)
    pjsip_to_hdr *to_hdr = (pjsip_to_hdr*)
        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TO, NULL);
    if (to_hdr) {
        pj_strdup(tdata->pool, &to_hdr->tag, &current_session.remote_tag);
    }

    PJ_LOG(3,(THIS_FILE, "Sending ACK with CSeq: %d", current_session.invite_cseq));

    // Send ACK
    status = pjsip_endpt_send_request_stateless(sip_endpt, tdata, NULL, NULL);
    if (status == PJ_SUCCESS) {
        PJ_LOG(3,(THIS_FILE, "ACK request sent successfully"));
    } else {
        PJ_LOG(1,(THIS_FILE, "Failed to send ACK request: %d", status));
    }

    return status;
}

sip_call_state_t sip_calling_get_state(void) {
    return current_session.state;
}

pj_bool_t sip_calling_is_call_active(void) {
    return (current_session.state != SIP_CALL_STATE_IDLE);
}

const sip_call_session_t* sip_calling_get_session(void) {
    if (current_session.state == SIP_CALL_STATE_IDLE) {
        return NULL;
    }
    return &current_session;
}

pj_bool_t sip_calling_check_timeout(void) {
    if (current_session.state == SIP_CALL_STATE_CALLING ||
        current_session.state == SIP_CALL_STATE_RINGING) {

        time_t now = time(NULL);
        if (now - current_session.call_start_time >= config.call_timeout_seconds) {
            PJ_LOG(2,(THIS_FILE, "Outgoing call timeout after %d seconds", config.call_timeout_seconds));
            set_call_state(SIP_CALL_STATE_IDLE);  // Go directly to IDLE
            return PJ_TRUE;
        }
    }
    return PJ_FALSE;
}

pj_bool_t sip_calling_handle_response(pjsip_rx_data *rdata) {
    pjsip_cid_hdr *cid_hdr;

    // Check if this response is for our outgoing call
    if (current_session.state == SIP_CALL_STATE_IDLE ||
        current_session.direction != SIP_CALL_DIRECTION_OUTGOING) {
        return PJ_FALSE;  // Not handling any outgoing calls
    }

    cid_hdr = (pjsip_cid_hdr*)pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
    if (!cid_hdr || pj_strcmp(&cid_hdr->id, &current_session.call_id) != 0) {
        return PJ_FALSE;  // Not our call
    }

    int status_code = rdata->msg_info.msg->line.status.code;
    PJ_LOG(3,(THIS_FILE, "Received response %d for outgoing call", status_code));

    // Check for duplicate 200 OK when already established
    if (status_code == 200 && current_session.state == SIP_CALL_STATE_ESTABLISHED) {
        PJ_LOG(3,(THIS_FILE, "Sending ACK for duplicate 200 OK"));
        send_ack_request(rdata);
        return PJ_TRUE;
    }

    if (status_code >= 100 && status_code < 200) {
        // Provisional responses
        if (status_code == 180 || status_code == 183) {
            set_call_state(SIP_CALL_STATE_RINGING);
        }
    } else if (status_code == 200) {
        // Call accepted
        PJ_LOG(3,(THIS_FILE, "Call accepted (200 OK)"));

        // Store dialog information from response
        store_dialog_info_from_response(rdata);

        // Parse SDP answer
        if (rdata->msg_info.msg->body && rdata->msg_info.msg->body->data) {
            char *sdp_content = (char*)rdata->msg_info.msg->body->data;
            sip_calling_parse_sdp_answer(sdp_content,
                                         &current_session.remote_rtp_port,
                                         &current_session.remote_dtmf_payload_type,
                                         &current_session.remote_video_rtp_port,
                                         &current_session.remote_video_payload_type);
        }

        // Send ACK
        send_ack_request(rdata);

        // Set state to established (this will trigger audio callback)
        set_call_state(SIP_CALL_STATE_ESTABLISHED);

    } else if (status_code >= 300) {
        // Call failed
        PJ_LOG(2,(THIS_FILE, "Call failed with status %d", status_code));
        set_call_state(SIP_CALL_STATE_IDLE);  // Go directly to IDLE
    }

    return PJ_TRUE;
}

pj_status_t sip_calling_create_sdp_offer(pj_pool_t* mem_pool,
                                        const char* local_ip,
                                        int local_rtp_port,
                                        int local_video_rtp_port,
                                        int video_payload_type,
                                        pj_str_t* sdp_str) {
    char sdp_body[768];
    int len;

    len = snprintf(sdp_body, sizeof(sdp_body),
                   "v=0\r\n"
                   "o=wibox 123456 654321 IN IP4 %s\r\n"
                   "s=Wibox Media Session\r\n"
                   "c=IN IP4 %s\r\n"
                   "t=0 0\r\n"
                   "m=audio %d RTP/AVP 8 101\r\n"
                   "a=rtpmap:8 PCMA/8000\r\n"
                   "a=rtpmap:101 telephone-event/8000\r\n"
                   "a=fmtp:101 0-16\r\n"
                   "a=sendrecv\r\n",
                   local_ip, local_ip, local_rtp_port);

    if (local_video_rtp_port > 0 && len > 0 && len < (int)sizeof(sdp_body)) {
        snprintf(sdp_body + len, sizeof(sdp_body) - len,
                 "m=video %d RTP/AVP %d\r\n"
                 "a=rtpmap:%d H264/90000\r\n"
                 "a=fmtp:%d packetization-mode=1;profile-level-id=42e01e\r\n"
                 "a=sendonly\r\n",
                 local_video_rtp_port,
                 video_payload_type,
                 video_payload_type,
                 video_payload_type);
    }

    PJ_LOG(3,(THIS_FILE, "Local SDP:\n%s", sdp_body));
    pj_strdup2(mem_pool, sdp_str, sdp_body);
    return PJ_SUCCESS;
}

static int parse_h264_payload_type(const char *sdp_content) {
    const char *video = strstr(sdp_content, "m=video ");
    const char *line;

    if (!video) {
        return 0;
    }

    line = video;
    while (line && *line) {
        const char *next = strstr(line, "\n");
        int payload_type = 0;
        char codec[32];

        if (line != video && strncmp(line, "m=", 2) == 0) {
            break;
        }
        if (sscanf(line, "a=rtpmap:%d %31[^/\r\n]", &payload_type, codec) == 2) {
            if (strcasecmp(codec, "H264") == 0) {
                return payload_type;
            }
        }

        if (!next) {
            break;
        }
        line = next + 1;
    }

    return 0;
}

static int parse_telephone_event_payload_type(const char *sdp_content) {
    const char *audio = strstr(sdp_content, "m=audio ");
    const char *line;

    if (!audio) {
        return 0;
    }

    line = audio;
    while (line && *line) {
        const char *next = strstr(line, "\n");
        int payload_type = 0;
        char codec[32];

        if (line != audio && strncmp(line, "m=", 2) == 0) {
            break;
        }
        if (sscanf(line, "a=rtpmap:%d %31[^/\r\n]", &payload_type, codec) == 2) {
            if (strcasecmp(codec, "telephone-event") == 0) {
                return payload_type;
            }
        }

        if (!next) {
            break;
        }
        line = next + 1;
    }

    return 0;
}

pj_status_t sip_calling_parse_sdp_answer(const char* sdp_content,
                                         int* remote_rtp_port,
                                         int* remote_dtmf_payload_type,
                                         int* remote_video_rtp_port,
                                         int* remote_video_payload_type) {
    if (!sdp_content || !remote_rtp_port || !remote_video_rtp_port) {
        return PJ_EINVAL;
    }

    *remote_rtp_port = 8000;  // Default
    if (remote_dtmf_payload_type) {
        *remote_dtmf_payload_type = 101;
    }
    *remote_video_rtp_port = 0;
    if (remote_video_payload_type) {
        *remote_video_payload_type = 0;
    }

    const char* media_line = strstr(sdp_content, "m=audio ");
    if (media_line) {
        int port;
        char protocol[32];
        if (sscanf(media_line, "m=audio %d %31s", &port, protocol) >= 1) {
            if (strstr(protocol, "RTP") != NULL) {
                *remote_rtp_port = port;
                PJ_LOG(3,(THIS_FILE, "Parsed remote RTP port: %d", port));
            }
        }
    }

    media_line = strstr(sdp_content, "m=video ");
    if (media_line) {
        int port;
        char protocol[32];
        if (sscanf(media_line, "m=video %d %31s", &port, protocol) >= 1) {
            if (strstr(protocol, "RTP") != NULL && port > 0) {
                *remote_video_rtp_port = port;
                PJ_LOG(3,(THIS_FILE, "Parsed remote video RTP port: %d", port));
            }
        }
    }

    if (remote_dtmf_payload_type) {
        int payload_type = parse_telephone_event_payload_type(sdp_content);
        if (payload_type > 0) {
            *remote_dtmf_payload_type = payload_type;
            PJ_LOG(3,(THIS_FILE, "Parsed remote telephone-event payload type: %d",
                      *remote_dtmf_payload_type));
        }
    }

    if (remote_video_payload_type) {
        *remote_video_payload_type = parse_h264_payload_type(sdp_content);
        if (*remote_video_payload_type > 0) {
            PJ_LOG(3,(THIS_FILE, "Parsed remote H264 payload type: %d",
                      *remote_video_payload_type));
        }
    }

    PJ_LOG(3,(THIS_FILE, "Remote RTP ports: audio=%d video=%d",
              *remote_rtp_port, *remote_video_rtp_port));
    return PJ_SUCCESS;
}

void sip_calling_cleanup(void) {
    if (current_session.state != SIP_CALL_STATE_IDLE) {
        sip_calling_terminate_call();
    }

    // Reset state
    memset(&current_session, 0, sizeof(sip_call_session_t));
    current_session.state = SIP_CALL_STATE_IDLE;
    current_session.direction = SIP_CALL_DIRECTION_NONE;

    state_callback = NULL;
    audio_callback = NULL;
    callback_user_data = NULL;

    PJ_LOG(3,(THIS_FILE, "Unified SIP calling module cleaned up"));
}
