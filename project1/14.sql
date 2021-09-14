SELECT V1.hometown
  FROM (SELECT T.hometown, count(*) AS cnt
          FROM Trainer T
         GROUP BY T.hometown
       ) V1
 WHERE cnt =
       (SELECT MAX(cnt)
          FROM (SELECT count(*) AS cnt
                  FROM Trainer T
                 GROUP BY T.hometown
               ) V2
       );
