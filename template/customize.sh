[ "$BOOTMODE" = true ] && [ "$ARCH" = "arm64" ] && [ "$API" -ge 35 ] || abort "! Unsupported environment"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/lib/libhostsredirect.so" 0 0 0444
