#!/bin/perl


open(OUT, ">>largest.espresso.log");

$ct = 0;

while($ct < 100) {
	$seed1 = int(rand(10000000));
	$seed2 = int(rand(10000000));

	if(system("LBM_SEED1=$seed1 LBM_SEED2=$seed2 LD_PRELOAD=util/libbrokenmalloc.so:./libdiefast.so espresso/espresso espresso/Input/largest.espresso")) {
		print "failure\n";
		print OUT $seed1, " ", $seed2, "\n";
		$ct++;
	} else {
		print "success\n";
	}
}

close(OUT);
