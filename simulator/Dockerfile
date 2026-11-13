FROM python:3.12-slim
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt
COPY simulate.py ./
CMD ["python", "simulate.py", "--base", "http://web", "--count", "6", "--interval", "5"]
