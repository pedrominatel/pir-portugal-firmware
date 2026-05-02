# pir-portugal-firmware
Perigo de Incêndio Rural (PIR) Continente via API do IPMA (Portugal)

## Sobre o Perigo de Incêndio Rural (PIR)

O [Instituto Português do Mar e da Atmosfera (IPMA)](https://www.ipma.pt/pt/riscoincendio/) disponibiliza diariamente a previsão do **Perigo de Incêndio Rural** para o território do Continente Português. Esta informação é essencial para a prevenção e combate a incêndios florestais.

A previsão é calculada com base no **índice FWI** (*Fire Weather Index*), que combina variáveis meteorológicas como temperatura, humidade relativa, velocidade do vento e precipitação.

### Níveis de Perigo de Incêndio

| Nível | Descrição      | FWI               |
|-------|----------------|-------------------|
| 1     | Reduzido       | FWI < 8.5         |
| 2     | Moderado       | 8.5 ≤ FWI < 17    |
| 3     | Elevado        | 17 ≤ FWI < 24.9   |
| 4     | Muito Elevado  | 24.9 ≤ FWI < 38.1 |
| 5     | Máximo         | FWI ≥ 38.1        |

### Informação adicional

- A previsão do Perigo de Incêndio Rural é emitida diariamente pelo IPMA em colaboração com a **ANEPC** (Autoridade Nacional de Emergência e Proteção Civil) e o **ICNF** (Instituto da Conservação da Natureza e das Florestas).
- Os dados são disponibilizados por distrito para o dia atual e os dias seguintes.
- Mais informações disponíveis em: [https://www.ipma.pt/pt/riscoincendio/](https://www.ipma.pt/pt/riscoincendio/)

## API do IPMA

Este firmware obtém os dados de Perigo de Incêndio Rural através da API pública do IPMA. A API disponibiliza os dados em formato JSON e é de acesso livre.

- **API base URL:** `https://api.ipma.pt/open-data/`
- **Documentação:** [https://api.ipma.pt/](https://api.ipma.pt/)
