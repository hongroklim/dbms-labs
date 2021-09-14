SELECT P.name
  FROM Pokemon P
     , (SELECT E1.after_id
          FROM Evolution E1
         WHERE NOT EXISTS
               (SELECT NULL
                  FROM Evolution E2
                 WHERE E2.after_id = E1.before_id
               )
       ) V1
 WHERE P.id = V1.after_id
 ORDER BY P.name;
