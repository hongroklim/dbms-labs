# Transaction Logging and Three-Pass Recovery

## Three-Pass Recovery

### Analysis

In order to distinguish winners and losers, `BEGIN`, `COMMIT` and `ROLLBACK` logs are focused. Like *parenthesis*, those are matched with corresponding transaction ID. If some transactions have startup but terminations don't appear, those transactions are treated as loser. On the other hand, the transactions which have either `COMMIT` or `ROLLBACK` will be winners.

### Redo

As traversing the log file from the head, `UPDATE` logs are re-done. If some pages in the disk have bigger `LSN` than logs', those statements are treated as `CONSIDER-REDO`. This is because those pages have been already updated and saved in the database. 

### Undo

In the reverse order of log file, losers' transactions are un-done. During the phase, successfully reverted transactions are appended with `COMPENSATE` logs with `next Undo LSN`. These additional compensate logs will help in the additional crashes by skipping themselves.