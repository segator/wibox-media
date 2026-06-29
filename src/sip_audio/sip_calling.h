#ifndef SIP_CALLING_H
#define SIP_CALLING_H

#include <pjsip.h>
#include <pjlib.h>

// Configuration structure
typedef struct {
    char target_uri[256];           // Target SIP URI for outgoing calls
    int call_timeout_seconds;       // Timeout for call establishment
    char local_ip[16];             // Local IP address
    int local_sip_port;            // Local SIP port
    int local_rtp_port;            // Local RTP port for audio
    int local_video_rtp_port;      // Local RTP port for video; 0 disables video SDP
    int video_payload_type;        // Dynamic RTP payload type for H.264
} sip_call_config_t;

// Call state enumeration
typedef enum {
    SIP_CALL_STATE_IDLE,
    SIP_CALL_STATE_CALLING,         // Outgoing call in progress
    SIP_CALL_STATE_RINGING,         // Outgoing call ringing
    SIP_CALL_STATE_INCOMING,        // Incoming call waiting for ACK
    SIP_CALL_STATE_ESTABLISHED,     // Call established (both directions)
    SIP_CALL_STATE_FAILED
} sip_call_state_t;

// Call direction enumeration
typedef enum {
    SIP_CALL_DIRECTION_NONE,
    SIP_CALL_DIRECTION_OUTGOING,
    SIP_CALL_DIRECTION_INCOMING
} sip_call_direction_t;

// Unified call session information
typedef struct {
    sip_call_state_t state;
    sip_call_direction_t direction;

    // Dialog information
    pj_str_t call_id;
    pj_str_t local_tag;             // Our tag (From tag for outgoing, To tag for incoming)
    pj_str_t remote_tag;            // Their tag (To tag for outgoing, From tag for incoming)
    pj_str_t remote_uri;            // Remote URI
    pj_str_t remote_contact;        // Remote Contact header

    // Audio information
    char remote_ip[16];
    int remote_rtp_port;
    int remote_video_rtp_port;

    // Timing
    time_t call_start_time;

    // Internal buffers
    char call_id_buf[128];
    char local_tag_buf[64];
    char remote_tag_buf[64];
    char remote_uri_buf[128];
    char remote_contact_buf[128];

    // Protocol details
    pj_uint32_t invite_cseq;        // CSeq of INVITE (for ACK)
} sip_call_session_t;

// Callback function types
typedef void (*sip_call_state_callback_t)(sip_call_state_t old_state, sip_call_state_t new_state, void* user_data);
typedef void (*sip_call_audio_callback_t)(const char* remote_ip, int remote_rtp_port,
                                          int remote_video_rtp_port, void* user_data);

// Function declarations

/**
 * Initialize unified SIP calling module
 */
pj_status_t sip_calling_init(const sip_call_config_t* config,
                            pjsip_endpoint* sip_endpt,
                            pj_pool_t* pool);

/**
 * Set callback functions
 */
void sip_calling_set_callbacks(sip_call_state_callback_t state_cb,
                              sip_call_audio_callback_t audio_cb,
                              void* user_data);

/**
 * Initiate an outgoing call
 */
pj_status_t sip_calling_make_call(void);

/**
 * Terminate current call (incoming or outgoing)
 */
pj_status_t sip_calling_terminate_call(void);

/**
 * Get current call state
 */
sip_call_state_t sip_calling_get_state(void);

/**
 * Check if any call is active
 */
pj_bool_t sip_calling_is_call_active(void);

/**
 * Get current call session info
 */
const sip_call_session_t* sip_calling_get_session(void);

/**
 * Check if call has timed out
 */
pj_bool_t sip_calling_check_timeout(void);

/**
 * Handle incoming SIP response (for outgoing calls)
 */
pj_bool_t sip_calling_handle_response(pjsip_rx_data *rdata);

/**
 * Handle incoming INVITE request
 */
pj_status_t sip_calling_handle_incoming_invite(pjsip_rx_data *rdata);

/**
 * Handle incoming ACK request
 */
pj_status_t sip_calling_handle_incoming_ack(pjsip_rx_data *rdata);

/**
 * Handle incoming BYE request
 */
pj_status_t sip_calling_handle_incoming_bye(pjsip_rx_data *rdata);

/**
 * Handle incoming CANCEL request
 */
pj_status_t sip_calling_handle_incoming_cancel(pjsip_rx_data *rdata);

/**
 * Create SDP offer
 */
pj_status_t sip_calling_create_sdp_offer(pj_pool_t* pool,
                                        const char* local_ip,
                                        int local_rtp_port,
                                        int local_video_rtp_port,
                                        int video_payload_type,
                                        pj_str_t* sdp_str);

/**
 * Parse SDP answer to extract RTP information
 */
pj_status_t sip_calling_parse_sdp_answer(const char* sdp_content,
                                         int* remote_rtp_port,
                                         int* remote_video_rtp_port);

/**
 * Cleanup SIP calling module
 */
void sip_calling_cleanup(void);

#endif // SIP_CALLING_H
