#!/usr/bin/env bash
# Mirrors the interesting part of the current workflow run's job logs into
# commit comments on the commit that was built:
#
#   * one summary comment per run (job, conclusion, number of compiler
#     warnings, and the warning lines themselves);
#   * one detail comment per failed job (errors with context + log tail).
#
# Why this exists: GitHub serves job logs and artifacts from a storage host
# that is not reachable from every development environment, while
# api.github.com usually is. Commit comments are readable through the API.
# This script is informational only and never changes the outcome of a run.
#
# Required environment: GH_TOKEN, REPO (owner/name), RUN_ID, RUN_ATTEMPT, SHA.
set -uo pipefail

: "${GH_TOKEN:?}" "${REPO:?}" "${RUN_ID:?}" "${RUN_ATTEMPT:?}" "${SHA:?}"

# Commit comment bodies are capped at 65536 characters; stay well below it.
readonly MAX_DIAG=42000
readonly MAX_WARN=6000
readonly MAX_TAIL=8000
readonly WARNING_RE='(^|[^a-zA-Z:])warning( [A-Z]+[0-9]+)?:'

# Drop the per-line timestamps and any ANSI colour/escape sequences.
clean_log() {
    sed -E -e 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:.]+Z ?//' -e 's/\x1b\[[0-9;?]*[ -\/]*[@-~]//g' "$1"
}

post_comment() {
    gh api -X POST "repos/$REPO/commits/$SHA/comments" -F body=@"$1" > /dev/null
}

# Every completed job of this run attempt except the reporting job itself.
gh api --paginate "repos/$REPO/actions/runs/$RUN_ID/attempts/$RUN_ATTEMPT/jobs" \
    --jq '.jobs[] | select(.status == "completed") | select(.name | startswith("CI report") | not) | "\(.id)\t\(.conclusion)\t\(.name)"' \
    > jobs.tsv || { echo "could not list jobs"; exit 0; }

if [ ! -s jobs.tsv ]; then
    echo "no completed jobs to report on"
    exit 0
fi

summary="summary.md"
warnings="warnings.md"
{
    echo "### CI report for run [${RUN_ID}](https://github.com/${REPO}/actions/runs/${RUN_ID}) (attempt ${RUN_ATTEMPT})"
    echo
    echo "| job | conclusion | compiler warnings |"
    echo "|-----|------------|-------------------|"
} > "$summary"
: > "$warnings"

while IFS=$'\t' read -r job_id conclusion job_name; do
    log="job-$job_id.log"
    clean="job-$job_id.txt"

    if ! gh api --allow-escape-sequences "repos/$REPO/actions/jobs/$job_id/logs" > "$log" 2> "$log.err"; then
        echo "| ${job_name} | ${conclusion} | (log not available: $(head -c 200 "$log.err")) |" >> "$summary"
        continue
    fi
    clean_log "$log" > "$clean"

    warn_count=$(grep -cE "$WARNING_RE" "$clean" || true)
    echo "| ${job_name} | ${conclusion} | ${warn_count:-0} |" >> "$summary"
    if [ "${warn_count:-0}" -gt 0 ]; then
        {
            echo "<details><summary>${job_name}: ${warn_count} warning line(s)</summary>"
            echo
            echo '```text'
            grep -nE "$WARNING_RE" "$clean" | cut -c1-400 | head -c "$MAX_WARN"
            echo '```'
            echo "</details>"
            echo
        } >> "$warnings"
    fi

    [ "$conclusion" = "success" ] && continue

    # Detail comment for a job that did not succeed.
    detail="detail-$job_id.md"
    diag=$(grep -nE -B2 -A12 \
        -e '[^a-zA-Z]error:' -e ': error ' -e '^error' -e 'Error:' -e 'CMake Error' \
        -e '^FAILED: ' -e 'fatal error' -e 'ninja: build stopped' \
        -e 'undefined (reference|symbol)' -e 'error LNK' -e 'ld: (error|warning)' \
        -e 'Process completed with exit code' -e '\[error\]' \
        "$clean" | cut -c1-600 | head -c "$MAX_DIAG")
    {
        echo "### CI report: ${job_name} — **${conclusion}**"
        echo
        echo "Run [${RUN_ID}](https://github.com/${REPO}/actions/runs/${RUN_ID}) attempt ${RUN_ATTEMPT}, job ${job_id}."
        echo
        echo "<details open><summary>Errors (with context)</summary>"
        echo
        echo '```text'
        if [ -n "$diag" ]; then echo "$diag"; else echo "(no line matched the error patterns)"; fi
        echo '```'
        echo "</details>"
        echo
        echo "<details><summary>Tail of the log</summary>"
        echo
        echo '```text'
        tail -n 60 "$clean" | cut -c1-600 | head -c "$MAX_TAIL"
        echo '```'
        echo "</details>"
    } > "$detail"

    if ! post_comment "$detail"; then
        echo "::warning::could not post the report for job ${job_name} (${job_id})"
        # Fallback: at least surface the first error lines as annotations.
        grep -nE '[^a-zA-Z]error:|CMake Error|fatal error' "$clean" 2>/dev/null | head -n 10 | while IFS= read -r line; do
            echo "::error::${job_name}: ${line:0:900}"
        done
    fi
done < jobs.tsv

{
    echo
    if [ -s "$warnings" ]; then cat "$warnings"; else echo "No compiler warnings."; fi
} >> "$summary"

if ! post_comment "$summary"; then
    echo "::warning::could not post the run summary"
fi
exit 0
