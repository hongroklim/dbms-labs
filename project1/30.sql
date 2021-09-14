SELECT P.name
  FROM Pokemon P
 WHERE UPPER(P.name) LIKE '%S'
 ORDER BY P.name;
