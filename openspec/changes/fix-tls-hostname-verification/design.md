# Design

Treat a non-success return from `SSL_set1_host` as a fatal TLS setup error. Release the SSL object, context and socket before returning. Keep chain verification and the existing post-handshake verification-result check unchanged. Extend the host TLS mock to inject hostname-setup failure and assert the download fails.
