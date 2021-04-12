## Troubleshooting

If you are running into issues setting up Caladan or while running experiments, the below maybe of help.

#### Error while running `rdma-core.sh`

You may run into the following error upon running `rdma-core.sh`:
```
../ibacm/src/libacm.c: In function 'ib_acm_connect_unix':
include/ccan/build_assert.h:23:26: error: size of unnamed array is negative
  do { (void) sizeof(char [1 - 2*!(cond)]); } while(0)
                          ^
../ibacm/src/libacm.c:105:3: note: in expansion of macro 'BUILD_ASSERT'
   BUILD_ASSERT(sizeof(IBACM_IBACME_SERVER_PATH) <=
   ^~~~~~~~~~~~
```

This error is caused because the path length of the working directory is too long <sup>1</sup> (see `pwd`). To resolve this, the length of the path needs to be reduced. When you run `pwd` in the `caladan-code` folder, the overall path length should not exceed 62 characters. You can reduce the length of the path by either: 
1. Placing the main blk-switch repository in a directory with as short path length as possible (for example, directly in your home folder)
2. Create a symlink to the `caladan/` directory in you home folder. For example: `ln -s <looong-path>/blk-switch/osdi21_artifact/caladan ~/caladan`. Then you can cd into `~/caladan` and run everything from there.

**Note:** Before re-running `rdma-core.sh` after failure, you need to clean up using `rm -rf rdma-core/`


### Footnotes
<sup>1</sup> At some point during the setup of rdma-core, a Unix domain socket whose name includes the path of the working directory is created. Unfortunately, there is a fixed length limit on the names of Unix domain sockets. Hence, long paths cause problems. 
