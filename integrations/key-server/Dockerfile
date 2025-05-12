FROM python:3.13-alpine3.21

WORKDIR /app

# Install system dependencies
RUN apk add --no-cache gnupg

# Install Python dependencies
COPY integrations/key-server/requirements.txt .
RUN pip install --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy application
COPY integrations/key-server/app.py .

# Create GnuPG home directory and set permissions
RUN mkdir -p /app/gnupg && chmod 700 /app/gnupg

# Expose the HKP port (11371)
EXPOSE 11371

# Run the service
ENTRYPOINT ["python", "app.py"]
