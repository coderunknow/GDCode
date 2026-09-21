#!/usr/bin/env bash
# Mirrors the interesting part of every job's log of the current workflow run
# into commit comments on the commit that was built.
#
# Why this exists: GitHub serves job logs and artifacts from a storage host
# that is not reachable from every development environment, while
# api.github.com usually is. Commit comments are readable through the API, so
# this job copies compiler diagnostics (errors with context, warnings, the tail
# of failed logs) there. It never changes the outcome of the run.
#
# Required environment: GH_TOKEN, REPO (owner/name), RUN_ID, RUN_ATTEMPT, SHA.
set -uo pipefail

: "${GH_TOKEN:?}" "${REPO:?}" "${RUN_ID:?}" "${RUN_ATTEMPT:?}" "${SHA:?}"

# Commit comment bodies are capped at 65536 characters; stay well below it.
readonly MAX_DIAG=42000
readonly MAX_WARN=8000
readonly MAX_TAIL=8000

strip_timestamps() {
    sed -E 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:.]+Z ?//' "$1"
}

# Every job of this run attempt except the reporting job itself.
gh api --paginate "repos/$REPO/actions/runs/$RUN_ID/attempts/$RUN_ATTEMPT/jobs" \
    --jq '.jobs[] | select(.status == "completed") | select(.name | startswith("CI report") | not) | "\(.id)\t\(.conclusion)\t\(.name)"' \
    > jobs.tsv || { echo "could not list jobs"; exit 0; }

if [ ! -s jobs.tsv ]; then
    echo "no completed jobs to report on"
    exit 0
fi

posted=0
while IFS=$'\t' read -r job_id conclusion job_name; do
    log="job-$job_id.log"
    clean="job-$job_id.txt"
    body="body-$job_id.md"

    {
        echo "### CI report: ${job_name} — **${conclusion}**"
        echo
        echo "Run [${RUN_ID}](https://github.com/${REPO}/actions/runs/${RUN_ID}) attempt ${RUN_ATTEMPT}, job ${job_id}."
        echo
    } > "$body"

    if ! gh api "repos/$REPO/actions/jobs/$job_id/logs" > "$log" 2> "$log.err"; then
        {
            echo "_Could not download the job log:_"
            echo '```'
            head -c 2000 "$log.err"
            echo '```'
        } >> "$body"
    else
        strip_timestamps "$log" > "$clean"

        if [ "$conclusion" != "success" ]; then
            diag=$(grep -nE -B2 -A12 \
                -e '[^a-zA-Z]error:' -e ': error ' -e '^error' -e 'Error:' -e 'CMake Error' \
                -e '^FAILED: ' -e 'fatal error' -e 'ninja: build stopped' \
                -e 'undefined (reference|symbol)' -e 'error LNK' -e 'ld: (error|warning)' \
                -e 'Process completed with exit code' -e '\[error\]' \
                "$clean" | cut -c1-600 | head -c "$MAX_DIAG")
            {
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
                echo
            } >> "$body"
        fi

        # Compiler warnings (clang/gcc "warning:", MSVC "warning Cxxxx:").
        warn_count=$(grep -cE '(^|[^a-zA-Z])warning( [A-Z]+[0-9]+)?:' "$clean" || true)
        {
            echo "<details><summary>Compiler warnings: ${warn_count} line(s)</summary>"
            echo
            echo '```text'
            if [ "${warn_count:-0}" -gt 0 ]; then
                grep -nE '(^|[^a-zA-Z])warning( [A-Z]+[0-9]+)?:' "$clean" | cut -c1-400 | head -c "$MAX_WARN"
            else
                echo "(none)"
            fi
            echo '```'
            echo "</details>"
        } >> "$body"
    fi

    if gh api -X POST "repos/$REPO/commits/$SHA/comments" -F body=@"$body" > /dev/null; then
        posted=$((posted + 1))
    else
        echo "::warning::could not post the report for job ${job_name} (${job_id})"
        # Fallback: at least surface the first error lines as annotations.
        grep -nE '[^a-zA-Z]error:|CMake Error|fatal error' "$clean" 2>/dev/null | head -n 10 | while IFS= read -r line; do
            echo "::error::${job_name}: ${line:0:900}"
        done
    fi
done < jobs.tsv

echo "posted ${posted} report comment(s)"
exit 0
