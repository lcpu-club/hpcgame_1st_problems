#!/usr/bin/php
<?php

if ($argc != 3) {
    die("USAGE: {$argv[0]} <OUTPUT_PATH> <N_RANGE>\n");
}

function i32_to_bytes(int $n): array
{
    $rslt = [];
    for ($i = 0; $i < 4; ++$i) {
        $rslt[] = $n & 255;
        $n >>= 8;
    }
    return $rslt;
}

function bytes_to_string(array $n): string
{
    $a = array_map(fn (int $num) => chr($num), $n);
    return join('', $a);
}

function i32_to_string(int $n): string
{
    return bytes_to_string(i32_to_bytes($n));
}

$f = fopen($argv[1], "w");

// $p = (int) $argv[2];
$n = (1 << ((int) trim($argv[2])));
$n += $n + rand(0, $n / 2);
$part = 1;
$n = floor($n / $part) * $part;
echo "n={$n}" . PHP_EOL;

// fwrite($f, i32_to_string($p));
fwrite($f, i32_to_string($n));
for ($i = 0; $i < ($n / $part); ++$i) {
    $m = i32_to_string(rand(0, 1 << 30));
    fwrite($f, $m);
}

fclose($f);
