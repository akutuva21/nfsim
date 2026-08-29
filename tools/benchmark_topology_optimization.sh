#!/bin/sh
set -eu

if [ "$#" -lt 7 ]; then
	cat >&2 <<'EOF'
usage: benchmark_topology_optimization.sh BASELINE_NFSIM CANDIDATE_NFSIM OUT_DIR POLY_XML TLBR_XML E1_XML EGFR_XML [SIM_SECONDS] [SEED] [TYPEII_XML]
EOF
	exit 2
fi

baseline=$1
candidate=$2
out_dir=$3
poly_xml=$4
tlbr_xml=$5
e1_xml=$6
egfr_xml=$7
sim_seconds=${8:-20}
seed=${9:-17}
script_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
typeii_xml=${10:-"$script_root/test/testSuite/t3.xml"}

mkdir -p "$out_dir"

run_profile_off() {
	variant=$1
	exe=$2
	name=$3
	xml=$4
	log="$out_dir/${variant}-${name}.log"

	"$exe" -xml "$xml" -sim "$sim_seconds" -oSteps 1 -seed "$seed" \
		-notf -o /dev/null >"$log" 2>&1
	awk -v variant="$variant" -v name="$name" '
		/You just simulated/ {
			cpu=$7
			sub(/s$/, "", cpu)
			printf "%s\t%s\tevents=%s\tsim_cpu_seconds=%s\n", variant, name, $4, cpu
			exit
		}
	' "$log"
}

run_exact() {
	variant=$1
	exe=$2
	name=$3
	xml=$4
	output="$out_dir/${variant}-${name}.gdat"

	"$exe" -xml "$xml" -sim "$sim_seconds" -oSteps 1 -seed "$seed" \
		-notf -o "$output" >"$out_dir/${variant}-${name}-exact.log" 2>&1
}

for variant_exe in "baseline $baseline" "candidate $candidate"; do
	variant=${variant_exe%% *}
	exe=${variant_exe#* }
	run_profile_off "$variant" "$exe" poly "$poly_xml"
	run_profile_off "$variant" "$exe" tlbr "$tlbr_xml"
	run_profile_off "$variant" "$exe" e1 "$e1_xml"
	run_profile_off "$variant" "$exe" egfr "$egfr_xml"
done

for name_xml in "poly $poly_xml" "tlbr $tlbr_xml" "e1 $e1_xml" "egfr $egfr_xml"; do
	name=${name_xml%% *}
	xml=${name_xml#* }
	run_exact baseline "$baseline" "$name" "$xml"
	run_exact candidate "$candidate" "$name" "$xml"
	if cmp -s "$out_dir/baseline-$name.gdat" "$out_dir/candidate-$name.gdat"; then
		echo "exact_output\t$name\tPASS"
	else
		echo "exact_output\t$name\tFAIL" >&2
		exit 1
	fi
done

for variant_exe in "baseline $baseline" "candidate $candidate"; do
	variant=${variant_exe%% *}
	exe=${variant_exe#* }
	profile="$out_dir/${variant}-poly.profile.tsv"
	"$exe" -xml "$poly_xml" -sim "$sim_seconds" -oSteps 1 -seed "$seed" \
		-notf -o /dev/null -profile "$profile" >"$out_dir/${variant}-poly-profile.log" 2>&1

	if [ "$variant" = candidate ]; then
		awk -F '\t' '
			$1 == "local_function_summary" && $2 != "rx_id" {
				seen=1
				if (($6 + 0) != 0 || ($7 + 0) != 0) bad=1
			}
			$1 == "connectivity_context" && $4 == "local_function" {
				if (($5 + 0) != 0 || ($6 + 0) != 0 || ($7 + 0) != 0) bad=1
			}
			END {
				if (!seen || bad) exit 1
				print "candidate_empty_typeII_update\tPASS"
			}
		' "$profile"
	else
		awk -F '\t' '
			$1 == "local_function_summary" && $2 != "rx_id" && ($6 + 0) > 0 { seen=1 }
			END {
				if (!seen) exit 1
				print "baseline_local_traversal\tPASS"
			}
		' "$profile"
	fi
done

for variant_exe in "baseline $baseline" "candidate $candidate"; do
	variant=${variant_exe%% *}
	exe=${variant_exe#* }
	profile="$out_dir/${variant}-typeII.profile.tsv"
	"$exe" -xml "$typeii_xml" -sim 0.1 -oSteps 1 -seed "$seed" \
		-notf -o /dev/null -profile "$profile" >"$out_dir/${variant}-typeII-profile.log" 2>&1

	if [ "$variant" = candidate ]; then
		awk -F '\t' '
			$1 == "connectivity_context" && $4 == "product_preparation" { prep_traversals += $5 }
			$1 == "local_function_summary" && $2 != "rx_id" {
				seen=1
				if (($4 + 0) == 0 || ($7 + 0) > 1) bad=1
			}
			$1 == "connectivity_context" && $4 == "local_function" && ($5 + 0) > 0 { traversals += $5 }
			END {
				if (!seen || bad || prep_traversals == 0 || traversals != 0) exit 1
				print "candidate_typeII_component_reuse\tPASS"
			}
		' "$profile"
	else
		awk -F '\t' '
			$1 == "local_function_summary" && $2 != "rx_id" {
				seen=1
				if (($6 + 0) == 0 || ($7 + 0) != 2) bad=1
			}
			$1 == "connectivity_context" && $4 == "local_function" && ($5 + 0) > 0 { traversals += $5 }
			END {
				if (!seen || bad || traversals == 0) exit 1
				print "baseline_typeII_duplicate_traversal\tPASS"
			}
		' "$profile"
	fi
done

"$candidate" -xml "$typeii_xml" -sim 0.1 -oSteps 1 -seed "$seed" \
	-notf -o "$out_dir/candidate-typeII.gdat" >"$out_dir/candidate-typeII.log" 2>&1
"$baseline" -xml "$typeii_xml" -sim 0.1 -oSteps 1 -seed "$seed" \
	-notf -o "$out_dir/baseline-typeII.gdat" >"$out_dir/baseline-typeII.log" 2>&1
if cmp -s "$out_dir/baseline-typeII.gdat" "$out_dir/candidate-typeII.gdat"; then
	echo "exact_output\ttypeII_fixture\tPASS"
else
	echo "exact_output\ttypeII_fixture\tFAIL" >&2
	exit 1
fi

for variant_exe in "baseline $baseline" "candidate $candidate"; do
	variant=${variant_exe%% *}
	exe=${variant_exe#* }
	"$exe" -xml "$typeii_xml" -sim 0.1 -oSteps 1 -seed "$seed" -utl 1 \
		-notf -o "$out_dir/${variant}-typeII-truncated.gdat" \
		-profile "$out_dir/${variant}-typeII-truncated.profile.tsv" \
		>"$out_dir/${variant}-typeII-truncated.log" 2>&1
done
if cmp -s "$out_dir/baseline-typeII-truncated.gdat" "$out_dir/candidate-typeII-truncated.gdat"; then
	echo "exact_output\ttypeII_truncated_fixture\tPASS"
else
	echo "exact_output\ttypeII_truncated_fixture\tFAIL" >&2
	exit 1
fi
awk -F '\t' '
	$1 == "connectivity_context" && $4 == "local_function" && ($5 + 0) > 0 { seen=1 }
	END {
		if (!seen) exit 1
		print "candidate_typeII_truncated_fallback\tPASS"
	}
' "$out_dir/candidate-typeII-truncated.profile.tsv"
