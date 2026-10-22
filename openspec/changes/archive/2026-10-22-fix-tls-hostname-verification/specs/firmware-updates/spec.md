## MODIFIED Requirements

### Requirement: HTTPS and checksum download

The updater SHALL verify TLS peer certificates and hostnames. It SHALL abort a connection if it cannot configure hostname verification, and SHALL NOT send an HTTP request on that connection. It SHALL follow at most five redirects, download the versioned image and `MD5SUMS`, and verify the image MD5 and size before touching flash.

#### Scenario: Hostname verification cannot be configured

- GIVEN the TLS library cannot configure hostname verification for the requested host
- WHEN the updater prepares an HTTPS connection
- THEN the connection is aborted before sending an HTTP request

#### Scenario: Untrusted certificate chain

- GIVEN the server certificate chain cannot be verified against the configured CA bundle
- WHEN the updater establishes an HTTPS connection
- THEN the TLS handshake fails
- AND the updater sends no HTTP request

#### Scenario: Download checksum differs

- GIVEN the downloaded image does not match its release MD5
- WHEN pre-flash verification runs
- THEN the updater deletes or rejects the image
- AND does not enter the flash phase
