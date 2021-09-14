SELECT T.name
  FROM Trainer T
     , (SELECT owner_id, count(*) AS cnt
          FROM CatchedPokemon
         GROUP BY owner_id
         HAVING count(*) >= 3
       ) V1
 WHERE T.id = V1.owner_id
 ORDER BY V1.cnt;
