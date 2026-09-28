Prebuilt SPI images for ROM boot tests live here.

The only checked-in image is `non_secure_boot.spi_preload`. It contains no
signature and needs no private key.

Secure-boot images are deliberately not checked in. Generate a fresh RSA-3072
key, matching Boot ROM, and signed SPI image for each run:

```bash
make -C fw/sep/bootcode secure_boot_ephemeral
```

The private key and all generated artifacts remain under the ignored
`fw/sep/bootcode/build/secure/` directory.

Keep these files in sync with:
- `fw/sep/bootcode/configs/non_secure_boot_test.yaml`
- `fw/sep/tests/bl1_pass_test/build/*`
