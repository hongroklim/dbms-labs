SELECT T.name, IFNULL(V1.cnt, 0)
  FROM Trainer T LEFT JOIN
       (SELECT CP.owner_id, count(*) AS cnt
          FROM CatchedPokemon CP
         GROUP BY CP.owner_id
       ) V1 ON T.id = V1.owner_id
 ORDER BY T.name;
