SELECT P.name
  FROM Pokemon P
 WHERE P.type = 'Water'
   AND NOT EXISTS
       (SELECT NULL
          FROM Evolution E
         WHERE E.before_id = P.id
       )
 ORDER BY P.name;
