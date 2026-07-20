# Changelog

## [0.18.0](https://github.com/segator/wibox-media/compare/v0.17.2...v0.18.0) (2026-07-20)


### Features

* configure SIP hangup on MQTT door unlock ([d583f1c](https://github.com/segator/wibox-media/commit/d583f1c96399c657039bf9c55d670ceb93e5f7b8))

## [0.17.2](https://github.com/segator/wibox-media/compare/v0.17.1...v0.17.2) (2026-07-18)


### Bug Fixes

* prevent Prometheus clients from stalling exporter ([2413370](https://github.com/segator/wibox-media/commit/2413370451cd84211add57f2844c782df3e2c781))
* prevent Prometheus clients from stalling exporter ([da06410](https://github.com/segator/wibox-media/commit/da06410fd972b80f90d33b03ac426d0aab50f18c))

## [0.17.1](https://github.com/segator/wibox-media/compare/v0.17.0...v0.17.1) (2026-07-15)


### Bug Fixes

* detach mqtt firmware updater ([2c763f4](https://github.com/segator/wibox-media/commit/2c763f48fb1838677e984a2337ebb5d9085dd491))
* detach MQTT firmware updater ([87d00f2](https://github.com/segator/wibox-media/commit/87d00f23780f8abc2ba138081d41f40db5812d97))

## [0.17.0](https://github.com/segator/wibox-media/compare/v0.16.2...v0.17.0) (2026-07-15)


### Features

* add call correlation and comprehensive verification ([377c363](https://github.com/segator/wibox-media/commit/377c36360cec360833003f07f065756efb2ee3b2))
* add correlated call IDs and E2E flows ([749c50c](https://github.com/segator/wibox-media/commit/749c50c560e02cf1546187c1334db5fb282aedb2))


### Bug Fixes

* arm hardware watchdog reset mode ([d7d5ba2](https://github.com/segator/wibox-media/commit/d7d5ba22c59d12e13049a811ac53dbc9a3259938))
* suspend release watchdog for runtime deploys ([adfe2a0](https://github.com/segator/wibox-media/commit/adfe2a0d4b44e5bd39f35b687c08a21712f8936c))
* target runtime supervisor by process identity ([91ff128](https://github.com/segator/wibox-media/commit/91ff12826c53a7b677ffbd46bc2123ff7a6b426c))

## [0.16.2](https://github.com/segator/wibox-media/compare/v0.16.1...v0.16.2) (2026-07-13)


### Bug Fixes

* tolerate unsupported MTD fsync ([c1cda6d](https://github.com/segator/wibox-media/commit/c1cda6d65ec3ffd3d4a824d34d8fdf026336d382))

## [0.16.1](https://github.com/segator/wibox-media/compare/v0.16.0...v0.16.1) (2026-07-13)


### Bug Fixes

* cancel remote ringing on handset answer ([86bbbf8](https://github.com/segator/wibox-media/commit/86bbbf8763c25506b67d5accac9eb4d19718bf15))
* cancel remote ringing on physical handset answer ([56e0e1e](https://github.com/segator/wibox-media/commit/56e0e1e9e8ac336b96246a9af453ed2dcf887a30))

## [0.16.0](https://github.com/segator/wibox-media/compare/v0.15.0...v0.16.0) (2026-07-11)


### Features

* add MQTT device reboot button ([8f2d1ba](https://github.com/segator/wibox-media/commit/8f2d1ba3e0e7a93d967301d603ffc2dc53a81a30))


### Bug Fixes

* label watchdog log rotator process ([e23e13b](https://github.com/segator/wibox-media/commit/e23e13b000a7281f99c2cdc1aaa79e21148708c0))

## [0.15.0](https://github.com/segator/wibox-media/compare/v0.14.0...v0.15.0) (2026-07-11)


### Features

* add hardware watchdog resilience ([3615ecc](https://github.com/segator/wibox-media/commit/3615ecce64575f521294efaa9c28684a7fb908d6))
* add hardware watchdog resilience ([70ba818](https://github.com/segator/wibox-media/commit/70ba818e653fd33277ca22516d7b90174169cb68))

## [0.14.0](https://github.com/segator/wibox-media/compare/v0.13.0...v0.14.0) (2026-07-06)


### Features

* add configurable outgoing SIP call flow ([7dc33d0](https://github.com/segator/wibox-media/commit/7dc33d022d0468c60c0b6f8491a1b10ed50d76bd))
* publish UART events over MQTT ([978dd18](https://github.com/segator/wibox-media/commit/978dd18ef0587c2b2ae55c2c0f375c4bb2b21332))

## [0.13.0](https://github.com/segator/wibox-media/compare/v0.12.0...v0.13.0) (2026-07-05)


### Features

* add developer-gated simulated doorbell trigger

## [0.12.0](https://github.com/segator/wibox-media/compare/v0.11.0...v0.12.0) (2026-07-03)


### Features

* tune h264 encoder idr handling ([20b26b7](https://github.com/segator/wibox-media/commit/20b26b7320526f7320ef2334b38e843cabca706d))


### Bug Fixes

* reduce intercom audio pops ([225f4c7](https://github.com/segator/wibox-media/commit/225f4c7a5ad282941c7aba36e30d128dfc1a3162))

## [0.11.0](https://github.com/segator/wibox-media/compare/v0.10.0...v0.11.0) (2026-07-03)


### Features

* add shared RTSP media service ([57a1526](https://github.com/segator/wibox-media/commit/57a152688adf5b473b14b943ce9476a3735d9c5c))

## [0.10.0](https://github.com/segator/wibox-media/compare/v0.9.0...v0.10.0) (2026-07-03)


### Features

* add automatic ring snapshots ([9183196](https://github.com/segator/wibox-media/commit/918319606a3691d509634b02ddf194edf8070e0f))


### Bug Fixes

* disable snapshot button while capture runs ([c2067a0](https://github.com/segator/wibox-media/commit/c2067a0982f7c0d6660bb246862cf7038bdba62b))

## [0.9.0](https://github.com/segator/wibox-media/compare/v0.8.0...v0.9.0) (2026-07-03)


### Features

* add snapshots and retained media controls ([0a0435b](https://github.com/segator/wibox-media/commit/0a0435b7b29315a8af1540452913ff9a2bcc0d67))

## [0.8.0](https://github.com/segator/wibox-media/compare/v0.7.0...v0.8.0) (2026-07-02)


### Features

* default video bitrate to 4096 ([4adbf88](https://github.com/segator/wibox-media/commit/4adbf88d9da1319dbc6ed46e3dc884ce0de7051d))
* improve video startup and bitrate ([4a100b9](https://github.com/segator/wibox-media/commit/4a100b91ebf68d902320ce0e24b2d13cc89598f1))

## [0.7.0](https://github.com/segator/wibox-media/compare/v0.6.1...v0.7.0) (2026-07-02)


### Features

* add f1 control and uart metrics ([eb63779](https://github.com/segator/wibox-media/commit/eb63779f0633e7cc8b2c8cc9c67d580a95f16630))

## [0.6.1](https://github.com/segator/wibox-media/compare/v0.6.0...v0.6.1) (2026-07-02)


### Bug Fixes

* clear legacy last ring discovery ([2afa6c1](https://github.com/segator/wibox-media/commit/2afa6c176e8e2df0ed14a752e2396365cd0687a3))

## [0.6.0](https://github.com/segator/wibox-media/compare/v0.5.3...v0.6.0) (2026-07-02)


### Features

* add call forward control switch ([d592e09](https://github.com/segator/wibox-media/commit/d592e090a74bc789ddd6e28b2f433148b813072e))

## [0.5.3](https://github.com/segator/wibox-media/compare/v0.5.2...v0.5.3) (2026-07-01)


### Bug Fixes

* respond to SIP OPTIONS requests ([90661f1](https://github.com/segator/wibox-media/commit/90661f1fb66ff24ea25df7162df98189eb83277b))

## [0.5.2](https://github.com/segator/wibox-media/compare/v0.5.1...v0.5.2) (2026-07-01)


### Bug Fixes

* clarify first install image transfer ([35da324](https://github.com/segator/wibox-media/commit/35da32463d14f2435b849fb11264072e02f9fac6))

## [0.5.1](https://github.com/segator/wibox-media/compare/v0.5.0...v0.5.1) (2026-07-01)


### Bug Fixes

* block duplicate firmware update installs ([71e17a2](https://github.com/segator/wibox-media/commit/71e17a2f8c62fe27e0b03f5bd93672ccaaf19392))

## [0.5.0](https://github.com/segator/wibox-media/compare/v0.4.10...v0.5.0) (2026-07-01)


### Features

* add firmware update refresh button ([5a87b6c](https://github.com/segator/wibox-media/commit/5a87b6c6b273bb314d9c04289926b404adbbe471))

## [0.4.10](https://github.com/segator/wibox-media/compare/v0.4.9...v0.4.10) (2026-07-01)


### Bug Fixes

* disable firmware update button when current ([79089a9](https://github.com/segator/wibox-media/commit/79089a988dc8042356ed83eae47c00812f8ae02c))

## [0.4.9](https://github.com/segator/wibox-media/compare/v0.4.8...v0.4.9) (2026-07-01)


### Bug Fixes

* make firmware updates use mtd erase path ([a705ef0](https://github.com/segator/wibox-media/commit/a705ef0dd3529b5cb41ac3081fd339f6da258d99))

## [0.4.8](https://github.com/segator/wibox-media/compare/v0.4.7...v0.4.8) (2026-07-01)


### Bug Fixes

* pin firmware build dependencies ([92306cf](https://github.com/segator/wibox-media/commit/92306cf82e23731815f2965db077a75c8a9e016c))
* use verified firmware update writes ([b692f7d](https://github.com/segator/wibox-media/commit/b692f7d4897abf83c14b93fff04e7b4d5d7f29bc))

## [0.4.3](https://github.com/segator/wibox-media/compare/wibox-media-v0.4.2...wibox-media-v0.4.3) (2026-06-30)


### Bug Fixes

* publish wall-clock event timestamps with offset ([df4040c](https://github.com/segator/wibox-media/commit/df4040c42512d9ecf3cdb5c3ec33b9ce35933c3f))
* use timezone config for HA event timestamps ([490e5de](https://github.com/segator/wibox-media/commit/490e5de006e7ef5df511707078efabfea148906e))

## [0.4.2](https://github.com/segator/wibox-media/compare/wibox-media-v0.4.1...wibox-media-v0.4.2) (2026-06-30)


### Bug Fixes

* cancel unanswered outgoing calls ([528cc42](https://github.com/segator/wibox-media/commit/528cc422c8818217ac0e82c24d36406a76bbc51f))
* clear call active after mqtt unlock ([af3dd52](https://github.com/segator/wibox-media/commit/af3dd522208ce7f4f520c22396fcab870e4d1b55))
* improve audio RTP pacing and HA timestamps ([c626807](https://github.com/segator/wibox-media/commit/c6268078fe71a1441ebe3f82c1bd696fc2987881))

## [0.4.1](https://github.com/segator/wibox-media/compare/wibox-media-v0.4.0...wibox-media-v0.4.1) (2026-06-30)


### Bug Fixes

* package current default media config ([aaf59aa](https://github.com/segator/wibox-media/commit/aaf59aa283ba5b64dc66e7d58e896b5849d6cf8c))

## [0.4.0](https://github.com/segator/wibox-media/compare/wibox-media-v0.3.1...wibox-media-v0.4.0) (2026-06-30)


### Features

* add Prometheus exporter ([5839e17](https://github.com/segator/wibox-media/commit/5839e17f0239b91f5d1c09242ea70e4077dcd5e0))

## [0.3.1](https://github.com/segator/wibox-media/compare/wibox-media-v0.3.0...wibox-media-v0.3.1) (2026-06-30)


### Bug Fixes

* wait for WiFi IP before SIP startup ([4be7cf7](https://github.com/segator/wibox-media/commit/4be7cf7f98c855557918cda9bc9a5f12a80fd8ac))

## [0.3.0](https://github.com/segator/wibox-media/compare/wibox-media-v0.2.0...wibox-media-v0.3.0) (2026-06-30)


### Features

* prepare WiBox deployment release ([7a01a3c](https://github.com/segator/wibox-media/commit/7a01a3cb13f70cd4dcf9c92c61a1c0cb8184501c))
* wibox-media - WiBox custom firmware builder with sofia_trace ([fd368bf](https://github.com/segator/wibox-media/commit/fd368bfce52b4151e5b57a9a3c77f9524b97b380))

## [0.2.0](https://github.com/segator/wibox-media/compare/wibox-media-v0.1.0...wibox-media-v0.2.0) (2026-06-30)


### Features

* wibox-media - WiBox custom firmware builder with sofia_trace ([fd368bf](https://github.com/segator/wibox-media/commit/fd368bfce52b4151e5b57a9a3c77f9524b97b380))
