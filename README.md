## Implementation of a Linux Weight Round-Robin scheduler.
## File changes
The main file of implementation is in /kernel/kernel/sched/wrr.c
## Investigate and Demo
First of all, test program is uploaded using `adb push`:
```
> adb push build-master/test/test /data/misc/test
```

### Weight Changing Test Scenario
```
> adb shell
(in phone)> /data/misc/test 0 <optional positive number>
```
Provide a positive number to change the maximum weight of each task applying wrr scheduling policy to be equal to it.

Also note that only root users will have the previledge of setting up wrr weight.

* If the weight is a small number (e.g. 10), the phone slightly lags.
* If the weight is a relatively large number (e.g. 100), the phone relatively lags.
* If the weight is a large number (e.g. 1000), the phone heavily lags.

### Idle Balance Test Scenario:
```
> adb shell
(in phone)> /data/misc/test 1
```
The total weights of the processes running on each CPU are roughly the same.
```
...
cpus:8
nr:2, weight:2
nr:2, weight:2
nr:1, weight:1
nr:2, weight:2
nr:1, weight:1
nr:1, weight:1
nr:1, weight:1
nr:2, weight:2
cpus:8
nr:1, weight:1
nr:1, weight:1
nr:1, weight:1
nr:1, weight:1
nr:0, weight:0
nr:1, weight:1
nr:0, weight:0
nr:0, weight:0
cpus:8
nr:2, weight:20
nr:2, weight:11
nr:2, weight:20
nr:1, weight:10
nr:1, weight:10
nr:2, weight:11
nr:1, weight:10
nr:1, weight:10
cpus:8
nr:2, weight:2
nr:1, weight:1
nr:2, weight:2
nr:1, weight:1
nr:2, weight:2
nr:2, weight:2
nr:1, weight:1
nr:2, weight:2
cpus:8
nr:2, weight:2
nr:1, weight:1
nr:2, weight:2
nr:2, weight:2
nr:2, weight:2
nr:2, weight:2
nr:2, weight:2
nr:1, weight:1
...
```
In addition, we add some printk statement and build an alternative kernel and the following screenshot shows that an idle CPU steals a task from another CPU.
```
[3626, Binder_F][   44.716375] Before Idle balance:Target CPU6, Source CPU5, Target CPU weight:0, Source CPU weight:2
[3626, Binder_F][   44.716390] After Idle balance:Target CPU6, Source CPU5, Target CPU weight:1, Source CPU weight:1
[2535, Binder_9][   44.716394] Before Idle balance:Target CPU3, Source CPU7, Target CPU weight:0, Source CPU weight:2
[2535, Binder_9][   44.716404] After Idle balance:Target CPU3, Source CPU7, Target CPU weight:1, Source CPU weight:1
[3133, Binder_3][   44.716548] Before Idle balance:Target CPU2, Source CPU4, Target CPU weight:0, Source CPU weight:2
[3133, Binder_3][   44.716559] After Idle balance:Target CPU2, Source CPU4, Target CPU weight:1, Source CPU weight:1
[2065, Binder_4][   44.716660] Before Idle balance:Target CPU7, Source CPU1, Target CPU weight:0, Source CPU weight:2
[2065, Binder_4][   44.716674] After Idle balance:Target CPU7, Source CPU1, Target CPU weight:1, Source CPU weight:1
[5194, Binder_1][   44.716677] Before Idle balance:Target CPU2, Source CPU4, Target CPU weight:0, Source CPU weight:2
[5194, Binder_1][   44.716685] After Idle balance:Target CPU2, Source CPU4, Target CPU weight:1, Source CPU weight:1
```

