"""Coverage describes observations, never inferred lamp operation."""
def coverage(rows, start, end, poll_seconds):
    observed, reachable, gaps = 0, 0, []
    cursor = start
    ordered = sorted(rows, key=lambda r: r['ts'])
    for i, row in enumerate(ordered):
        ts = row['ts']
        following = ordered[i+1]['ts'] if i+1 < len(ordered) else end
        # Allow normal poll jitter; long gaps receive only one poll of coverage.
        span = following-ts if 0 <= following-ts <= 2*poll_seconds else poll_seconds
        a, b = max(start, ts, cursor), min(end, ts+span)
        if b <= a:
            continue
        if a > cursor:
            gaps.append([cursor, a])
        observed += b-a
        if row['data'] is not None:
            reachable += b-a
        cursor = b
    if cursor < end:
        gaps.append([cursor, end])
    return {'start': start, 'end': end, 'observed_seconds': observed,
            'reachable_seconds': reachable, 'unreachable_seconds': observed-reachable,
            'no_observer_seconds': end-start-observed, 'gaps': gaps}
