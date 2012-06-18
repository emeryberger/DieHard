#!/usr/bin/perl

$ITERS = 500;

$INJECTOR="util/libbrokenmalloc.so";

$PROG = "espresso/espresso espresso/Input/largest.espresso";
#$PROG = "/usr/bin/perl perfect.pl -m 4 -b 3";
#$PROG = "/nfs/sting/scratch1/gnovark/heaplayers/benchmarks/cfrac/cfrac 41757646344123832613190542166099121";
#$PROG = "/nfs/sting/scratch1/gnovark/heaplayers/benchmarks/roboop/bench";

open(OUT, ">>overflow_data.log");

for($i = 0; $i < $ITERS; $i++) {
    system("./clean.sh");
    system("killall espresso");
    system("killall dieinject");

    #$seed1 = int(rand(1000000));
    #$seed2 = int(rand(1000000));

    $seed1=966388;
    $seed2=489137;

    $ret = 0;
    $j = 0;

    $ret = system("LBM_SEED1=$seed1 LBM_SEED2=$seed2 DH_COREFILE=DHcore-$j LD_PRELOAD=$INJECTOR:./libdiefast.so $PROG");
    
    print("ret = $ret\n");
    
    if($ret == 0) {
        next;
    }

    $death_time = `./core_helper DHcore-0`;

    print "DEATH TIME IS $death_time\n";

    $j++;
    $jtext = sprintf("%02d",$j);
    $cmd = "LBM_SEED1=$seed1 LBM_SEED2=$seed2 DH_COREFILE=DHcore-$jtext DH_DEATHTIME=$death_time LD_PRELOAD=$INJECTOR:./libdiefast.so $PROG";
    print $cmd;
    $ret = system($cmd);

    # patch!
    while(!`egrep "overflow|dangle" foo.patch`) {
        `rm foo.patch`;
        $j++;
        $jtext = sprintf("%02d",$j);
        $cmd = "LBM_SEED1=$seed1 LBM_SEED2=$seed2 DH_COREFILE=DHcore-$jtext DH_DEATHTIME=$death_time LD_PRELOAD=$INJECTOR:./libdiefast.so $PROG";
        print $cmd;
        $ret = system($cmd);
        `./patcher/patcher DHcore*`;
    }

    print OUT "$seed1 $seed2 $j\n";

    $foo = `LBM_SEED1=$seed1 LBM_SEED2=$seed2 LD_PRELOAD=$INJECTOR:./libdiefast.so $PROG 2>&1 | grep retention`;

    print OUT $foo;
    
    exit;
    
    next;

    $successes = 0;

#     for($k = 0; $k < 10; $k++) {
#         $dhseed = int(rand(1000000));
#         $ret = system("LBM_SEED1=$seed1 LBM_SEED2=$seed2 DH_COREFILE=/dev/null DH_SEED=$dhseed LD_PRELOAD=util/libbrokenmalloc.so:./libdiefast.so espresso/espresso -t espresso/Input/largest.espresso");
#         if($ret == 0) {
#             $successes++;
#         }
#     }


    print $seed1, " ", $seed2, " ", $j, " ", $successes, "\n";
}

close(OUT);
