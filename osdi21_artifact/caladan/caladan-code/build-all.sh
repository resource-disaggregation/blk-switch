make clean && make
pushd ksched
make clean & make
popd
make -C bindings/cc clean && make -C bindings/cc
make -C apps/bench clean && make -C apps/bench storage_client
rm apps/storage_service/storage_server_ssd >/dev/null 2>&1 && make -C apps/storage_service clean && make -C apps/storage_service && mv apps/storage_service/storage_server apps/storage_service/storage_server_ssd
make -C apps/storage_service clean && make -C apps/storage_service CONFIG_RAM_DISK=y