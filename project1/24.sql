SELECT P.name
  FROM Pokemon P
 WHERE UPPER(SUBSTR(P.name, 1, 1)) IN ('A', 'E', 'I', 'O', 'U')
 ORDER BY P.name;
